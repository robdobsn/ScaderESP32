#!/usr/bin/env python3
"""
WS2811 Timing Analysis Script

Analyzes logic analyzer CSV data exported from libsigrok4DSL to verify:
1. WS2811 timing compliance (according to datasheet)
2. Timing variability statistics
3. AutoID sequence correctness
4. Sync and single-LED timing periods

WS2811 Protocol (from datasheet):
- T0H: 0.5us ± 150ns (high time for 0 bit)
- T0L: 2.0us ± 150ns (low time for 0 bit)
- T1H: 1.2us ± 150ns (high time for 1 bit)
- T1L: 1.3us ± 150ns (low time for 1 bit)
- RES: >50us (reset/latch time)
"""

import csv
import argparse
import sys
import os
from pathlib import Path
from typing import List, Tuple, Dict, Optional, Iterator
from dataclasses import dataclass, field
from collections import defaultdict, deque
import statistics
import time


@dataclass
class WS2811Timing:
    """WS2811 timing specifications in microseconds"""
    def __init__(self, high_speed: bool = False):
        # High speed mode halves nominal timing, but tolerance (±150ns) remains constant
        speed_factor = 0.5 if high_speed else 1.0
        tolerance = 0.15  # ±150ns tolerance in microseconds
        
        # T0H: 0 bit high time
        self.T0H_NOM = 0.5 * speed_factor
        self.T0H_MIN = self.T0H_NOM - tolerance
        self.T0H_MAX = self.T0H_NOM + tolerance
        
        # T0L: 0 bit low time
        self.T0L_NOM = 2.0 * speed_factor
        self.T0L_MIN = self.T0L_NOM - tolerance
        self.T0L_MAX = self.T0L_NOM + tolerance
        
        # T1H: 1 bit high time
        self.T1H_NOM = 1.2 * speed_factor
        self.T1H_MIN = self.T1H_NOM - tolerance
        self.T1H_MAX = self.T1H_NOM + tolerance
        
        # T1L: 1 bit low time
        self.T1L_NOM = 1.3 * speed_factor
        self.T1L_MIN = self.T1L_NOM - tolerance
        self.T1L_MAX = self.T1L_NOM + tolerance
        
        self.RES_MIN = 50.0  # Reset time minimum (unchanged in high speed mode)


@dataclass
class BitTiming:
    """Represents timing for a single bit"""
    high_time: float  # in microseconds
    low_time: float   # in microseconds
    bit_value: int    # 0 or 1
    start_time: float # absolute time in seconds
    is_valid: bool = True
    error_msg: str = ""


@dataclass
class LEDFrame:
    """Represents a complete LED frame (24 bits for WS2811)"""
    bits: List[BitTiming] = field(default_factory=list)
    start_time: float = 0.0
    green: int = 0
    red: int = 0
    blue: int = 0
    
    def is_complete(self) -> bool:
        return len(self.bits) == 24
    
    def decode_colors(self):
        """Decode GRB format"""
        if self.is_complete():
            bit_values = [bit.bit_value for bit in self.bits]
            self.green = int(''.join(map(str, bit_values[0:8])), 2)
            self.red = int(''.join(map(str, bit_values[8:16])), 2)
            self.blue = int(''.join(map(str, bit_values[16:24])), 2)


@dataclass
class Statistics:
    """Timing statistics with incremental computation"""
    count: int = 0
    sum: float = 0.0
    sum_sq: float = 0.0
    min_val: float = float('inf')
    max_val: float = float('-inf')
    samples: deque = field(default_factory=lambda: deque(maxlen=10000))  # Keep limited samples for median
    
    def add(self, value: float):
        self.count += 1
        self.sum += value
        self.sum_sq += value * value
        self.min_val = min(self.min_val, value)
        self.max_val = max(self.max_val, value)
        self.samples.append(value)
    
    def get_stats(self) -> Dict:
        if self.count == 0:
            return {}
        
        mean = self.sum / self.count
        variance = (self.sum_sq / self.count) - (mean * mean)
        stdev = variance ** 0.5 if variance > 0 else 0
        
        return {
            'count': self.count,
            'mean': mean,
            'median': statistics.median(self.samples) if self.samples else mean,
            'stdev': stdev,
            'min': self.min_val,
            'max': self.max_val
        }


class WS2811Analyzer:
    """Main analyzer class with streaming analysis"""
    
    def __init__(self, csv_file: Path, num_leds_chain0: int = 1000, num_leds_chain1: int = 1000, 
                 max_errors: int = 1000, start_time: float = None, end_time: float = None,
                 high_speed: bool = None):
        self.csv_file = csv_file
        self.num_leds_chain0 = num_leds_chain0
        self.num_leds_chain1 = num_leds_chain1
        self.total_leds = num_leds_chain0 + num_leds_chain1
        self.max_errors = max_errors
        self.start_time = start_time
        self.end_time = end_time
        self.high_speed = high_speed
        self.timing = None  # Will be set after speed detection if needed
        
        # Statistics
        self.bit0_high_stats = Statistics()
        self.bit0_low_stats = Statistics()
        self.bit1_high_stats = Statistics()
        self.bit1_low_stats = Statistics()
        
        # Errors (limited to save memory)
        self.timing_errors = deque(maxlen=max_errors)
        self.total_errors = 0
        
        # Segments and patterns
        self.segments = []
        self.reset_periods = []
        
        # Progress tracking
        self.file_size = 0
        self.bytes_processed = 0
        self.lines_processed = 0
        self.last_progress_time = 0
    
    def show_progress(self, force: bool = False):
        """Show progress bar"""
        current_time = time.time()
        if not force and current_time - self.last_progress_time < 0.5:
            return
        
        self.last_progress_time = current_time
        
        if self.file_size > 0:
            percent = min(100, (self.bytes_processed / self.file_size) * 100)
            bar_width = 40
            filled = int(bar_width * percent / 100)
            bar = '█' * filled + '░' * (bar_width - filled)
            
            mb_processed = self.bytes_processed / (1024 * 1024)
            mb_total = self.file_size / (1024 * 1024)
            lines_m = self.lines_processed / 1_000_000
            
            print(f'\r  Progress: [{bar}] {percent:.1f}% ({mb_processed:.1f}/{mb_total:.1f} MB, {lines_m:.1f}M lines)', 
                  end='', flush=True)
    
    def parse_csv_streaming(self) -> Iterator[Tuple[float, int, int]]:
        """Parse CSV file incrementally, yielding data points one at a time"""
        self.file_size = os.path.getsize(self.csv_file)
        self.bytes_processed = 0
        self.lines_processed = 0
        
        with open(self.csv_file, 'r') as f:
            for line in f:
                self.lines_processed += 1
                line_len = len(line)
                self.bytes_processed += line_len
                
                # Skip comment lines and empty lines
                line = line.strip()
                if not line or line.startswith(';') or line.startswith('Time'):
                    continue
                
                try:
                    # Parse CSV manually (simple comma split)
                    parts = line.split(',')
                    time_val = float(parts[0])
                    
                    # Skip data before start_time
                    if self.start_time is not None and time_val < self.start_time:
                        continue
                    
                    # Stop processing after end_time
                    if self.end_time is not None and time_val > self.end_time:
                        break
                    
                    ch0 = int(parts[1])
                    ch1 = int(parts[2]) if len(parts) > 2 else 0
                    yield (time_val, ch0, ch1)
                except (ValueError, IndexError):
                    continue
    
    def process_pulses_streaming(self, channel: int = 0) -> Iterator[Tuple[float, float, int]]:
        """
        Extract pulses from compressed CSV data incrementally
        Yields (start_time, duration, level) tuples
        """
        data_iterator = self.parse_csv_streaming()
        
        prev_point = next(data_iterator, None)
        if prev_point is None:
            return
        
        pulse_count = 0
        
        for current_point in data_iterator:
            time, ch0, ch1 = prev_point
            next_time = current_point[0]
            
            level = ch0 if channel == 0 else ch1
            duration = next_time - time
            
            pulse_count += 1
            if pulse_count % 100000 == 0:
                self.show_progress()
            
            yield (time, duration, level)
            
            prev_point = current_point
        
        self.show_progress(force=True)
        print()  # New line after progress bar
    
    def auto_detect_speed_mode(self, sample_size: int = 1000) -> bool:
        """
        Auto-detect whether data is high-speed or standard speed mode
        Returns True for high-speed, False for standard speed
        """
        print("\n  Auto-detecting speed mode...")
        
        pulse_iterator = self.process_pulses_streaming(channel=0)
        
        high_times = []
        low_times = []
        prev_pulse = None
        
        # Collect sample pulses
        for pulse in pulse_iterator:
            start_time, duration, level = pulse
            
            if prev_pulse is None:
                prev_pulse = pulse
                continue
            
            prev_start, prev_duration, prev_level = prev_pulse
            
            # Look for high-low sequences
            if prev_level == 1 and level == 0:
                high_us = prev_duration * 1_000_000
                low_us = duration * 1_000_000
                
                high_times.append(high_us)
                low_times.append(low_us)
                
                if len(high_times) >= sample_size:
                    break
                
                prev_pulse = None
            else:
                prev_pulse = pulse
        
        if len(high_times) < 10:
            print("  Warning: Insufficient data for speed detection, defaulting to standard speed")
            return False
        
        # Calculate average timing
        avg_high = sum(high_times) / len(high_times)
        avg_low = sum(low_times) / len(low_times)
        
        # Compare against nominal timings for both modes
        # Standard mode nominals: T0H=0.5, T1H=1.2, average ~0.85
        # High-speed mode nominals: T0H=0.25, T1H=0.6, average ~0.425
        standard_nominal = (0.5 + 1.2) / 2
        highspeed_nominal = (0.25 + 0.6) / 2
        
        dist_to_standard = abs(avg_high - standard_nominal)
        dist_to_highspeed = abs(avg_high - highspeed_nominal)
        
        is_high_speed = dist_to_highspeed < dist_to_standard
        
        mode_name = "high-speed (2x)" if is_high_speed else "standard"
        print(f"  Detected mode: {mode_name} (avg high: {avg_high:.3f}us, avg low: {avg_low:.3f}us)")
        
        return is_high_speed
    
    def decode_bits_streaming(self) -> Iterator[BitTiming]:
        """
        Decode WS2811 bits from pulses incrementally
        Yields BitTiming objects one at a time
        """
        pulse_iterator = self.process_pulses_streaming(channel=0)
        
        bit_count = 0
        prev_pulse = None
        
        for pulse in pulse_iterator:
            start_time, duration, level = pulse
            
            if prev_pulse is None:
                prev_pulse = pulse
                continue
            
            prev_start, prev_duration, prev_level = prev_pulse
            
            # Look for high-low sequence
            if prev_level == 1 and level == 0:
                # Convert to microseconds
                high_us = prev_duration * 1_000_000
                low_us = duration * 1_000_000
                
                # Determine if it's a 0 or 1 bit based on timing
                bit_value, is_valid, error_msg = self.classify_bit(high_us, low_us)
                
                bit = BitTiming(
                    high_time=high_us,
                    low_time=low_us,
                    bit_value=bit_value,
                    start_time=prev_start,
                    is_valid=is_valid,
                    error_msg=error_msg
                )
                
                # Update statistics
                if bit_value == 0:
                    self.bit0_high_stats.add(high_us)
                    self.bit0_low_stats.add(low_us)
                else:
                    self.bit1_high_stats.add(high_us)
                    self.bit1_low_stats.add(low_us)
                
                if not is_valid:
                    self.total_errors += 1
                    self.timing_errors.append({
                        'time': prev_start,
                        'high_us': high_us,
                        'low_us': low_us,
                        'bit_value': bit_value,
                        'error': error_msg
                    })
                
                bit_count += 1
                yield bit
                
                # Skip the next pulse since we've consumed it as the low part
                prev_pulse = None
            else:
                prev_pulse = pulse
    
    def classify_bit(self, high_us: float, low_us: float) -> Tuple[int, bool, str]:
        """
        Classify a bit as 0 or 1 based on timing
        Returns (bit_value, is_valid, error_message)
        """
        timing = self.timing
        epsilon = 1e-9  # Small tolerance for floating point comparison
        
        # Check if high time is closer to T0H or T1H
        dist_to_0 = abs(high_us - timing.T0H_NOM)
        dist_to_1 = abs(high_us - timing.T1H_NOM)
        
        if dist_to_0 < dist_to_1:
            # Likely a 0 bit
            bit_value = 0
            errors = []
            
            if not (timing.T0H_MIN - epsilon <= high_us <= timing.T0H_MAX + epsilon):
                errors.append(f"T0H out of range: {high_us:.6f}us (expected {timing.T0H_MIN:.4f}-{timing.T0H_MAX:.4f})")
            
            if not (timing.T0L_MIN - epsilon <= low_us <= timing.T0L_MAX + epsilon):
                errors.append(f"T0L out of range: {low_us:.6f}us (expected {timing.T0L_MIN:.4f}-{timing.T0L_MAX:.4f})")
            
            is_valid = len(errors) == 0
            error_msg = "; ".join(errors) if errors else ""
            
        else:
            # Likely a 1 bit
            bit_value = 1
            errors = []
            
            if not (timing.T1H_MIN - epsilon <= high_us <= timing.T1H_MAX + epsilon):
                errors.append(f"T1H out of range: {high_us:.3f}us (expected {timing.T1H_MIN:.4f}-{timing.T1H_MAX:.4f})")
            
            if not (timing.T1L_MIN - epsilon <= low_us <= timing.T1L_MAX + epsilon):
                errors.append(f"T1L out of range: {low_us:.3f}us (expected {timing.T1L_MIN:.4f}-{timing.T1L_MAX:.4f})")
            
            is_valid = len(errors) == 0
            error_msg = "; ".join(errors) if errors else ""
        
        return bit_value, is_valid, error_msg
    
    def group_into_frames_streaming(self) -> Iterator[LEDFrame]:
        """Group bits into 24-bit LED frames incrementally"""
        bit_iterator = self.decode_bits_streaming()
        
        current_frame = LEDFrame()
        frame_count = 0
        
        for bit in bit_iterator:
            current_frame.bits.append(bit)
            
            if len(current_frame.bits) == 1:
                current_frame.start_time = bit.start_time
            
            if current_frame.is_complete():
                current_frame.decode_colors()
                frame_count += 1
                
                if frame_count % 1000 == 0:
                    print(f'\r  Decoded {frame_count} frames...', end='', flush=True)
                
                yield current_frame
                current_frame = LEDFrame()
        
        print()  # New line
    
    def detect_reset_periods_streaming(self):
        """Detect reset/latch periods (greater than 50us low) while processing"""
        timing = self.timing
        reset_count = 0
        
        print("\n  Detecting reset periods...")
        for start_time, duration, level in self.process_pulses_streaming(channel=0):
            duration_us = duration * 1_000_000
            
            if level == 0 and duration_us >= timing.RES_MIN:
                self.reset_periods.append((start_time, duration_us))
                reset_count += 1
                
                if reset_count % 10 == 0:
                    print(f'\r  Found {reset_count} reset periods...', end='', flush=True)
        
        print(f'\r  Found {len(self.reset_periods)} reset periods total')
        return self.reset_periods
    
    def analyze_autoid_sequence_streaming(self):
        """Analyze the AutoID sequence while streaming frames"""
        print("\n" + "="*80)
        print("AUTOID SEQUENCE ANALYSIS")
        print("="*80)
        
        frame_iterator = self.group_into_frames_streaming()
        
        current_segment = []
        reset_idx = 0
        total_frames = 0
        first_frame_time = None
        last_frame_time = None
        
        print("\n  Analyzing segments...")
        
        for frame in frame_iterator:
            total_frames += 1
            
            if first_frame_time is None:
                first_frame_time = frame.start_time
            last_frame_time = frame.start_time
            
            # Check if there's a reset before this frame
            while reset_idx < len(self.reset_periods) and self.reset_periods[reset_idx][0] < frame.start_time:
                if current_segment:
                    self.segments.append(current_segment)
                    
                    # Analyze segment if it's one of the first few
                    if len(self.segments) <= 20:
                        self._analyze_segment(len(self.segments), current_segment)
                    
                    current_segment = []
                reset_idx += 1
            
            current_segment.append(frame)
        
        if current_segment:
            self.segments.append(current_segment)
            if len(self.segments) <= 20:
                self._analyze_segment(len(self.segments), current_segment)
        
        # Summary
        print(f"\n{'='*80}")
        print(f"Total segments recorded: {len(self.segments)}")
        print(f"Total LED frames: {total_frames}")
        if first_frame_time and last_frame_time:
            print(f"Recording duration: {last_frame_time - first_frame_time:.2f} seconds")
    
    def _analyze_segment(self, seg_idx: int, segment: List[LEDFrame]):
        """Analyze a single segment"""
        print(f"\n--- Segment {seg_idx} ({len(segment)} LEDs) ---")
        
        # Check if all LEDs are on (sync)
        all_on = all(f.red == 255 and f.green == 255 and f.blue == 255 for f in segment)
        all_off = all(f.red == 0 and f.green == 0 and f.blue == 0 for f in segment)
        
        if all_on:
            duration = segment[-1].start_time - segment[0].start_time
            print(f"  → SYNC: All LEDs ON (duration: {duration*1000:.1f}ms)")
        elif all_off:
            print(f"  → SYNC: All LEDs OFF")
        else:
            # Find lit LEDs
            lit_leds = [(i, f) for i, f in enumerate(segment) if f.red > 0 or f.green > 0 or f.blue > 0]
            
            if lit_leds:
                lit_indices = [i for i, _ in lit_leds]
                print(f"  → PATTERN: {len(lit_leds)} LED(s) lit at indices {lit_indices[:10]}{'...' if len(lit_indices) > 10 else ''}")
                
                # Show color pattern
                for i, frame in lit_leds[:5]:  # Show first 5
                    print(f"     LED {i}: R={frame.red:3d} G={frame.green:3d} B={frame.blue:3d}")
            else:
                print(f"  → All LEDs OFF")
    
    def print_timing_statistics(self):
        """Print timing statistics"""
        print("\n" + "="*80)
        print("TIMING STATISTICS")
        print("="*80)
        
        print("\nBit 0 Timing (High):")
        self._print_stats(self.bit0_high_stats.get_stats(), "us")
        
        print("\nBit 0 Timing (Low):")
        self._print_stats(self.bit0_low_stats.get_stats(), "us")
        
        print("\nBit 1 Timing (High):")
        self._print_stats(self.bit1_high_stats.get_stats(), "us")
        
        print("\nBit 1 Timing (Low):")
        self._print_stats(self.bit1_low_stats.get_stats(), "us")
        
        print("\nReset Periods:")
        if self.reset_periods:
            reset_durations = [duration for _, duration in self.reset_periods]
            reset_stats = Statistics()
            for d in reset_durations:
                reset_stats.add(d)
            self._print_stats(reset_stats.get_stats(), "us")
        else:
            print("  No reset periods found")
    
    def _print_stats(self, stats: Dict, unit: str):
        """Helper to print statistics"""
        if not stats:
            print("  No data")
            return
        
        print(f"  Count:  {stats['count']}")
        print(f"  Mean:   {stats['mean']:.3f} {unit}")
        print(f"  Median: {stats['median']:.3f} {unit}")
        print(f"  StdDev: {stats['stdev']:.3f} {unit}")
        print(f"  Min:    {stats['min']:.3f} {unit}")
        print(f"  Max:    {stats['max']:.3f} {unit}")
    
    def print_timing_errors(self, show_count: int = 50):
        """Print timing errors"""
        print("\n" + "="*80)
        print("TIMING ERRORS")
        print("="*80)
        
        if self.total_errors == 0:
            print("\n✓ No timing errors detected!")
            return
        
        print(f"\nFound {self.total_errors} timing errors total")
        errors_to_show = min(show_count, len(self.timing_errors))
        print(f"Showing first {errors_to_show} errors:\n")
        
        for i, error in enumerate(list(self.timing_errors)[:errors_to_show]):
            print(f"Error {i+1} at t={error['time']:.6f}s:")
            print(f"  High: {error['high_us']:.3f}us, Low: {error['low_us']:.3f}us")
            print(f"  Bit value: {error['bit_value']}, Error: {error['error']}")
        
        if self.total_errors > errors_to_show:
            print(f"\n... and {self.total_errors - errors_to_show} more errors")
        
        # Error rate
        total_bits = (self.bit0_high_stats.count + self.bit1_high_stats.count)
        error_rate = (self.total_errors / total_bits * 100) if total_bits > 0 else 0
        print(f"\nError rate: {error_rate:.3f}% ({self.total_errors}/{total_bits} bits)")
    
    def analyze(self):
        """Main analysis function with streaming"""
        print(f"\nAnalyzing: {self.csv_file}")
        print(f"LED configuration: Chain 0: {self.num_leds_chain0}, Chain 1: {self.num_leds_chain1}")
        
        # Auto-detect speed mode if not specified
        if self.high_speed is None:
            self.high_speed = self.auto_detect_speed_mode()
        
        # Initialize timing based on detected or specified mode
        self.timing = WS2811Timing(high_speed=self.high_speed)
        
        print(f"Timing mode: {'High-speed (2x)' if self.high_speed else 'Standard'}")
        
        if self.start_time is not None or self.end_time is not None:
            time_range = f"{self.start_time or 0:.2f}s to {self.end_time or '∞'}s"
            print(f"Time range: {time_range}")
        
        print("="*80)
        
        # First pass: detect reset periods
        print("\nPass 1: Detecting reset periods")
        self.detect_reset_periods_streaming()
        
        # Second pass: decode bits, group frames, and analyze sequence
        print("\nPass 2: Decoding bits and analyzing sequence")
        self.analyze_autoid_sequence_streaming()
        
        # Print results
        self.print_timing_statistics()
        self.print_timing_errors(show_count=50)
        
        print("\n" + "="*80)
        print("ANALYSIS COMPLETE")
        print("="*80)


def main():
    parser = argparse.ArgumentParser(
        description='Analyze WS2811 timing data from logic analyzer CSV export',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s data.csv
  %(prog)s data.csv --leds-chain0 1000 --leds-chain1 1000
  %(prog)s ../data/capture.csv
        """
    )
    
    parser.add_argument(
        'csv_file',
        type=Path,
        nargs='?',
        help='Path to CSV file (default: searches in ../data/ folder)'
    )
    
    parser.add_argument(
        '--leds-chain0',
        type=int,
        default=1000,
        help='Number of LEDs in chain 0 (default: 1000)'
    )
    
    parser.add_argument(
        '--leds-chain1',
        type=int,
        default=1000,
        help='Number of LEDs in chain 1 (default: 1000)'
    )
    
    parser.add_argument(
        '--data-dir',
        type=Path,
        default=None,
        help='Override default data directory'
    )
    
    parser.add_argument(
        '--start-time',
        type=float,
        default=None,
        help='Start processing from this time in seconds (default: from beginning)'
    )
    
    parser.add_argument(
        '--end-time',
        type=float,
        default=None,
        help='Stop processing at this time in seconds (default: until end)'
    )
    
    parser.add_argument(
        '--duration',
        type=float,
        default=None,
        help='Process only this duration in seconds (alternative to --end-time)'
    )
    
    speed_group = parser.add_mutually_exclusive_group()
    speed_group.add_argument(
        '--high-speed',
        action='store_true',
        help='Force high-speed mode timing (all timings halved except reset)'
    )
    speed_group.add_argument(
        '--low-speed',
        action='store_true',
        help='Force standard/low-speed mode timing'
    )
    
    args = parser.parse_args()
    
    # Determine speed mode: None = auto-detect, True = high-speed, False = standard
    if args.high_speed:
        speed_mode = True
    elif args.low_speed:
        speed_mode = False
    else:
        speed_mode = None  # Auto-detect
    
    # Calculate end_time from duration if specified
    end_time = args.end_time
    if args.duration is not None:
        start = args.start_time or 0
        end_time = start + args.duration
    
    # Determine CSV file path
    if args.csv_file:
        csv_file = args.csv_file
    else:
        # Default to ../data/ folder relative to script
        script_dir = Path(__file__).parent
        data_dir = args.data_dir if args.data_dir else script_dir.parent / 'data'
        
        if not data_dir.exists():
            print(f"Error: Data directory not found: {data_dir}")
            sys.exit(1)
        
        # Find CSV files
        csv_files = list(data_dir.glob('*.csv'))
        
        if not csv_files:
            print(f"Error: No CSV files found in {data_dir}")
            sys.exit(1)
        
        # Use the most recent one
        csv_file = sorted(csv_files, key=lambda p: p.stat().st_mtime, reverse=True)[0]
        print(f"Using most recent CSV file: {csv_file}")
    
    if not csv_file.exists():
        print(f"Error: File not found: {csv_file}")
        sys.exit(1)
    
    # Run analysis
    analyzer = WS2811Analyzer(
        csv_file=csv_file,
        num_leds_chain0=args.leds_chain0,
        num_leds_chain1=args.leds_chain1,
        start_time=args.start_time,
        end_time=end_time,
        high_speed=speed_mode
    )
    
    analyzer.analyze()


if __name__ == '__main__':
    main()
