#!/usr/bin/env python3
"""
LED Pixel Timing Analysis Script

Analyzes logic analyzer CSV data exported from libsigrok4DSL to verify:
1. LED pixel timing compliance (WS2811, WS2812B, SK6812, etc.)
2. Timing variability statistics
3. AutoID sequence correctness
4. Sync and single-LED timing periods

Supports multiple LED chip types with configurable timing parameters.
Timing values can be specified as fixed values with tolerance or as ranges.
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


# LED Chip Timing Database (all values in microseconds)
# Format: 'chip_name': {'T0H': (min, max), 'T0L': (min, max), 'T1H': (min, max), 'T1L': (min, max), 'RES': min_value}
CHIP_TIMINGS = {
    'WS2811': {'T0H': (0.35, 0.65), 'T0L': (1.85, 2.15), 'T1H': (1.05, 1.35), 'T1L': (1.15, 1.45), 'RES': 50.0},
    'WS2811-HS': {'T0H': (0.1, 0.4), 'T0L': (0.85, 1.15), 'T1H': (0.45, 0.75), 'T1L': (0.5, 0.8), 'RES': 50.0},
    'WS2812B': {'T0H': (0.25, 0.55), 'T0L': (0.7, 1.0), 'T1H': (0.65, 0.95), 'T1L': (0.3, 0.6), 'RES': 50.0},
    'SK6812': {'T0H': (0.15, 0.45), 'T0L': (0.75, 1.05), 'T1H': (0.45, 0.75), 'T1L': (0.45, 0.75), 'RES': 80.0},
    'SK6812-MINI': {'T0H': (0.15, 0.45), 'T0L': (0.75, 1.05), 'T1H': (0.45, 0.75), 'T1L': (0.45, 0.75), 'RES': 80.0},
    'SK6812-SIDE': {'T0H': (0.15, 0.45), 'T0L': (0.75, 1.05), 'T1H': (0.45, 0.75), 'T1L': (0.45, 0.75), 'RES': 80.0},
    'SK6805-EC15': {'T0H': (0.2, 0.4), 'T0L': (0.8, 1.0), 'T1H': (0.58, 1.0), 'T1L': (0.2, 0.4), 'RES': 80.0},
    'SK6805-EC20': {'T0H': (0.2, 0.4), 'T0L': (0.8, 1.0), 'T1H': (0.58, 1.0), 'T1L': (0.2, 0.4), 'RES': 80.0},
    'SK6805-EC14': {'T0H': (0.2, 0.4), 'T0L': (0.8, 1.0), 'T1H': (0.65, 1.0), 'T1L': (0.2, 0.4), 'RES': 200.0},
    'APA104': {'T0H': (0.2, 0.5), 'T0L': (1.21, 1.51), 'T1H': (1.21, 1.51), 'T1L': (0.2, 0.5), 'RES': 24.0},
}


@dataclass
class LEDTiming:
    """LED pixel timing specifications in microseconds"""
    def __init__(self, chip: str = 'WS2812B', 
                 t0h: Optional[Tuple[float, float]] = None,
                 t0l: Optional[Tuple[float, float]] = None,
                 t1h: Optional[Tuple[float, float]] = None,
                 t1l: Optional[Tuple[float, float]] = None,
                 reset: Optional[float] = None):
        
        # Start with chip defaults
        if chip not in CHIP_TIMINGS:
            raise ValueError(f"Unknown chip type: {chip}. Available: {', '.join(CHIP_TIMINGS.keys())}")
        
        defaults = CHIP_TIMINGS[chip]
        self.chip_name = chip
        
        # T0H: 0 bit high time
        self.T0H_MIN, self.T0H_MAX = t0h if t0h else defaults['T0H']
        self.T0H_NOM = (self.T0H_MIN + self.T0H_MAX) / 2
        
        # T0L: 0 bit low time
        self.T0L_MIN, self.T0L_MAX = t0l if t0l else defaults['T0L']
        self.T0L_NOM = (self.T0L_MIN + self.T0L_MAX) / 2
        
        # T1H: 1 bit high time
        self.T1H_MIN, self.T1H_MAX = t1h if t1h else defaults['T1H']
        self.T1H_NOM = (self.T1H_MIN + self.T1H_MAX) / 2
        
        # T1L: 1 bit low time
        self.T1L_MIN, self.T1L_MAX = t1l if t1l else defaults['T1L']
        self.T1L_NOM = (self.T1L_MIN + self.T1L_MAX) / 2
        
        # Reset time minimum
        self.RES_MIN = reset if reset is not None else defaults['RES']


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


class LEDTimingAnalyzer:
    """Main analyzer class with streaming analysis"""
    
    def __init__(self, csv_file: Path, num_leds_chain0: int = 1000, num_leds_chain1: int = 1000, 
                 max_errors: int = 1000, start_time: float = None, end_time: float = None,
                 chip: str = 'WS2812B', t0h: Optional[Tuple[float, float]] = None,
                 t0l: Optional[Tuple[float, float]] = None, t1h: Optional[Tuple[float, float]] = None,
                 t1l: Optional[Tuple[float, float]] = None, reset: Optional[float] = None):
        self.csv_file = csv_file
        self.num_leds_chain0 = num_leds_chain0
        self.num_leds_chain1 = num_leds_chain1
        self.total_leds = num_leds_chain0 + num_leds_chain1
        self.max_errors = max_errors
        self.start_time = start_time
        self.end_time = end_time
        self.chip = chip
        
        # Create timing object with chip defaults and any overrides
        self.timing = LEDTiming(chip=chip, t0h=t0h, t0l=t0l, t1h=t1h, t1l=t1l, reset=reset)
        
        # Statistics
        self.bit0_high_stats = Statistics()
        self.bit0_low_stats = Statistics()
        self.bit1_high_stats = Statistics()
        self.bit1_low_stats = Statistics()
        
        # End-of-segment transition timing
        self.end_of_segment_transitions = Statistics()
        self.transition_count = 0
        
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
    
    def decode_bits_streaming(self) -> Iterator[BitTiming]:
        """
        Decode LED bits from pulses incrementally
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
                
                # Check for end-of-segment transition (extended low period before reset)
                # This can occur at any bit position, not just the last bit of a frame
                is_transition = False
                if not is_valid:
                    # Check if this is a T0L or T1L error with extended low period
                    # (typical transition pattern: ~2.08-2.14µs instead of ~1.0µs)
                    timing = self.timing
                    epsilon = 1e-9
                    
                    # Transition signature: low period extended beyond normal range but below reset threshold
                    # and within the expected transition timing range (~2.0-2.2µs)
                    if bit_value == 0:
                        # Check if low time is extended beyond T0L_MAX (but less than reset threshold)
                        if (low_us > timing.T0L_MAX + epsilon and 
                            low_us < timing.RES_MIN and
                            1.8 <= low_us <= 2.5):  # Transition timing signature
                            is_transition = True
                            self.end_of_segment_transitions.add(low_us)
                            self.transition_count += 1
                    else:
                        # Check if low time is extended beyond T1L_MAX (but less than reset threshold)
                        if (low_us > timing.T1L_MAX + epsilon and 
                            low_us < timing.RES_MIN and
                            1.8 <= low_us <= 2.5):  # Transition timing signature
                            is_transition = True
                            self.end_of_segment_transitions.add(low_us)
                            self.transition_count += 1
                
                bit = BitTiming(
                    high_time=high_us,
                    low_time=low_us,
                    bit_value=bit_value,
                    start_time=prev_start,
                    is_valid=is_valid if not is_transition else True,  # Don't mark transition as error
                    error_msg=error_msg if not is_transition else ""
                )
                
                # Update statistics
                if bit_value == 0:
                    self.bit0_high_stats.add(high_us)
                    # Don't include transition timing in normal statistics
                    if not is_transition:
                        self.bit0_low_stats.add(low_us)
                else:
                    self.bit1_high_stats.add(high_us)
                    if not is_transition:
                        self.bit1_low_stats.add(low_us)
                
                # Only count as error if it's not a transition
                if not is_valid and not is_transition:
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
        
        # Print end-of-segment transitions if any were detected
        if self.transition_count > 0:
            print("\nEnd-of-Segment Transitions:")
            print(f"  Description: Extended low period on last bit of frame before reset")
            print(f"  Occurrences: {self.transition_count}")
            trans_stats = self.end_of_segment_transitions.get_stats()
            if trans_stats:
                print(f"  Mean:   {trans_stats['mean']:.3f} us")
                print(f"  StdDev: {trans_stats['stdev']:.3f} us")
                print(f"  Min:    {trans_stats['min']:.3f} us")
                print(f"  Max:    {trans_stats['max']:.3f} us")
            print(f"  Note: These are hardware transition delays, not protocol violations")
    
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
        print(f"Chip type: {self.timing.chip_name}")
        print(f"Timing - T0H: {self.timing.T0H_MIN:.2f}-{self.timing.T0H_MAX:.2f}us, "
              f"T0L: {self.timing.T0L_MIN:.2f}-{self.timing.T0L_MAX:.2f}us, "
              f"T1H: {self.timing.T1H_MIN:.2f}-{self.timing.T1H_MAX:.2f}us, "
              f"T1L: {self.timing.T1L_MIN:.2f}-{self.timing.T1L_MAX:.2f}us, "
              f"RES: >{self.timing.RES_MIN:.0f}us")
        
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
        description='Analyze LED pixel timing data from logic analyzer CSV export',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"""
Examples:
  %(prog)s data.csv
  %(prog)s data.csv --chip WS2812B
  %(prog)s data.csv --chip SK6812 --leds-chain0 1000
  %(prog)s data.csv --chip WS2811-HS --t0h 0.1 0.4
  %(prog)s ../data/capture.csv --duration 10

Available chip types: {', '.join(CHIP_TIMINGS.keys())}
        """
    )
    
    parser.add_argument(
        'csv_file',
        type=Path,
        nargs='?',
        help='Path to CSV file (default: searches in ../data/ folder)'
    )
    
    parser.add_argument(
        '--chip',
        type=str,
        default='WS2812B',
        choices=list(CHIP_TIMINGS.keys()),
        help='LED chip type (default: WS2812B)'
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
    
    # Timing override arguments
    parser.add_argument(
        '--t0h',
        nargs=2,
        type=float,
        metavar=('MIN', 'MAX'),
        help='Override T0H timing range in microseconds (e.g., --t0h 0.25 0.55)'
    )
    
    parser.add_argument(
        '--t0l',
        nargs=2,
        type=float,
        metavar=('MIN', 'MAX'),
        help='Override T0L timing range in microseconds'
    )
    
    parser.add_argument(
        '--t1h',
        nargs=2,
        type=float,
        metavar=('MIN', 'MAX'),
        help='Override T1H timing range in microseconds'
    )
    
    parser.add_argument(
        '--t1l',
        nargs=2,
        type=float,
        metavar=('MIN', 'MAX'),
        help='Override T1L timing range in microseconds'
    )
    
    parser.add_argument(
        '--reset',
        type=float,
        metavar='MIN',
        help='Override reset timing minimum in microseconds'
    )
    
    args = parser.parse_args()
    
    # Parse timing overrides
    t0h = tuple(args.t0h) if args.t0h else None
    t0l = tuple(args.t0l) if args.t0l else None
    t1h = tuple(args.t1h) if args.t1h else None
    t1l = tuple(args.t1l) if args.t1l else None
    
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
    analyzer = LEDTimingAnalyzer(
        csv_file=csv_file,
        num_leds_chain0=args.leds_chain0,
        num_leds_chain1=args.leds_chain1,
        start_time=args.start_time,
        end_time=end_time,
        chip=args.chip,
        t0h=t0h,
        t0l=t0l,
        t1h=t1h,
        t1l=t1l,
        reset=args.reset
    )
    
    analyzer.analyze()


if __name__ == '__main__':
    main()
