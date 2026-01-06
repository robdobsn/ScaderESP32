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


class LEDDecoder:
    """State machine for decoding LED data from channel transitions"""
    def __init__(self, timing: LEDTiming):
        self.timing = timing
        self.state = 'RESET'  # States: RESET, HIGH, LOW
        self.high_start = None
        self.high_duration = None
        self.current_frame = LEDFrame()
        self.bit_count = 0
        self.frame_count = 0
        
    def process_transition(self, time: float, level: int, duration: float) -> Optional[LEDFrame]:
        """
        Process a channel transition and return a complete frame if ready.
        
        Args:
            time: Absolute time of the transition (seconds)
            level: Signal level (0=low, 1=high)
            duration: Duration of this pulse (seconds)
        
        Returns:
            LEDFrame if a complete frame is ready, None otherwise
        """
        duration_us = duration * 1_000_000
        
        # State machine
        if self.state == 'RESET':
            if level == 1:
                # Start of new data
                self.state = 'HIGH'
                self.high_start = time
                self.high_duration = duration_us
            # Stay in RESET for any low pulse
            return None
        
        elif self.state == 'HIGH':
            if level == 0:
                # Check if this is a reset period
                if duration_us >= self.timing.RES_MIN:
                    # Reset - discard any partial frame and reset state
                    self.state = 'RESET'
                    if len(self.current_frame.bits) > 0:
                        # Partial frame discarded
                        self.current_frame = LEDFrame()
                    return None
                else:
                    # Valid low period - we have a complete bit
                    self.state = 'LOW'
                    low_duration = duration_us
                    
                    # Classify the bit
                    bit_value, is_valid, error_msg = self._classify_bit(self.high_duration, low_duration)
                    
                    bit = BitTiming(
                        high_time=self.high_duration,
                        low_time=low_duration,
                        bit_value=bit_value,
                        start_time=self.high_start,
                        is_valid=is_valid,
                        error_msg=error_msg
                    )
                    
                    # Add bit to current frame
                    if len(self.current_frame.bits) == 0:
                        self.current_frame.start_time = self.high_start
                    
                    self.current_frame.bits.append(bit)
                    self.bit_count += 1
                    
                    # Check if frame is complete
                    if self.current_frame.is_complete():
                        self.current_frame.decode_colors()
                        completed_frame = self.current_frame
                        self.current_frame = LEDFrame()
                        self.frame_count += 1
                        return completed_frame
            # else: level==1 means we got another high without a low - error, but stay in HIGH
            return None
        
        elif self.state == 'LOW':
            if level == 1:
                # Next bit starts
                self.state = 'HIGH'
                self.high_start = time
                self.high_duration = duration_us
            # else: level==0 means we got another low without a high - shouldn't happen
            return None
        
        return None
    
    def _classify_bit(self, high_us: float, low_us: float) -> Tuple[int, bool, str]:
        """Classify a bit as 0 or 1 based on timing"""
        # Epsilon accounts for sampling jitter at 20MHz (sample period = 0.05us)
        # Using 0.1us to provide margin for timing variations
        epsilon = 0.1
        
        # Check if high time is closer to T0H or T1H
        dist_to_0 = abs(high_us - self.timing.T0H_NOM)
        dist_to_1 = abs(high_us - self.timing.T1H_NOM)
        
        if dist_to_0 < dist_to_1:
            # Bit 0
            bit_value = 0
            errors = []
            if not (self.timing.T0H_MIN - epsilon <= high_us <= self.timing.T0H_MAX + epsilon):
                errors.append(f"T0H out of range: {high_us:.6f}us")
            if not (self.timing.T0L_MIN - epsilon <= low_us <= self.timing.T0L_MAX + epsilon):
                errors.append(f"T0L out of range: {low_us:.6f}us")
            return bit_value, len(errors) == 0, "; ".join(errors)
        else:
            # Bit 1
            bit_value = 1
            errors = []
            if not (self.timing.T1H_MIN - epsilon <= high_us <= self.timing.T1H_MAX + epsilon):
                errors.append(f"T1H out of range: {high_us:.6f}us")
            if not (self.timing.T1L_MIN - epsilon <= low_us <= self.timing.T1L_MAX + epsilon):
                errors.append(f"T1L out of range: {low_us:.6f}us")
            return bit_value, len(errors) == 0, "; ".join(errors)


class AutoIDStateMachine:
    """State machine to verify AutoID sequence pattern
    
    States:
    - START: Initial state, counting sync frames (expecting all ON/OFF alternation)
    - SEQUENCE: Processing sequential LED frames (0, 1, 2, ...)
    - SYNC: Processing sync frame (all ON or all OFF)
    - ERROR: Terminal error state
    
    Expected pattern:
    - Start with 3 sync events (all ON/OFF alternation)
    - Then 10 sequential LEDs followed by a sync, repeating
    - LED indices increment continuously across chain transitions
    """
    
    def __init__(self, num_leds_chain0: int = 1000, num_leds_chain1: int = 1000, 
                 max_lit_leds_tolerance: int = 20):
        self.state = 'START'
        self.num_leds_chain0 = num_leds_chain0
        self.num_leds_chain1 = num_leds_chain1
        self.max_lit_leds_tolerance = max_lit_leds_tolerance
        
        # START state counters
        self.start_sync_count = 0
        self.start_expecting_off = False  # False = expecting ON, True = expecting OFF
        
        # SEQUENCE state counters
        self.current_led_index = 0  # Expected LED index (0-999 chain0, 0-999 chain1)
        self.leds_since_sync = 0  # Count LEDs since last sync
        
        # SYNC state
        self.sync_expecting_off = False  # False = expecting ON, True = expecting OFF
        
        # Error tracking
        self.error_messages: List[str] = []
        self.segment_count = 0
        
        # Anomaly tracking
        self.anomaly_count = 0
        self.max_anomaly_logs = 50
        self.anomaly_details: List[Dict] = []
        
    def process_segment(self, segment: List[LEDFrame], seg_time: float) -> None:
        """Process a segment and update state machine
        
        Args:
            segment: List of LED frames in this segment
            seg_time: Start time of segment in seconds
        """
        self.segment_count += 1
        
        if self.state == 'ERROR':
            return  # No further processing once in error state
        
        # Classify the segment
        seg_type = self._classify_segment(segment)
        
        # Handle UNKNOWN segments - could be anomalies if in SEQUENCE state
        if seg_type == 'UNKNOWN':
            # In SEQUENCE state, check if this is a tolerable anomaly (multiple LEDs lit)
            if self.state == 'SEQUENCE':
                lit_leds = [(i, f) for i, f in enumerate(segment) if f.red > 0 or f.green > 0 or f.blue > 0]
                num_lit = len(lit_leds)
                
                # If within tolerance (2-20 LEDs), treat as anomaly and continue
                if 2 <= num_lit <= self.max_lit_leds_tolerance:
                    self._log_multi_led_anomaly(seg_time, segment, lit_leds)
                    self.current_led_index += 1
                    self.leds_since_sync += 1
                    return
                
                # Too many LEDs lit (>20), treat as error
                self._record_error(f"Segment {self.segment_count} at {seg_time:.3f}s: Too many LEDs lit ({num_lit} > {self.max_lit_leds_tolerance})")
                self.state = 'ERROR'
                return
            else:
                # In other states, UNKNOWN is an error
                self._record_error(f"Segment {self.segment_count} at {seg_time:.3f}s: Unknown segment type (not ALL_OFF, ALL_ON, or ONE_LED)")
                self.state = 'ERROR'
                return
        
        # Process based on current state
        if self.state == 'START':
            self._process_start_state(seg_type, seg_time)
        elif self.state == 'SEQUENCE':
            self._process_sequence_state(seg_type, segment, seg_time)
        elif self.state == 'SYNC':
            self._process_sync_state(seg_type, seg_time)
    
    def _classify_segment(self, segment: List[LEDFrame]) -> str:
        """Classify segment as ALL_OFF, ALL_ON, ONE_LED, or UNKNOWN"""
        if not segment:
            return 'UNKNOWN'
        
        # Check for all OFF
        all_off = all(f.red == 0 and f.green == 0 and f.blue == 0 for f in segment)
        if all_off:
            return 'ALL_OFF'
        
        # Check for all ON (any uniform non-zero color)
        first_color = (segment[0].red, segment[0].green, segment[0].blue)
        if first_color != (0, 0, 0):
            all_same = all((f.red, f.green, f.blue) == first_color for f in segment)
            if all_same:
                return 'ALL_ON'
        
        # Check for exactly one LED ON
        lit_leds = [i for i, f in enumerate(segment) if f.red > 0 or f.green > 0 or f.blue > 0]
        if len(lit_leds) == 1:
            return 'ONE_LED'
        
        return 'UNKNOWN'
    
    def _process_start_state(self, seg_type: str, seg_time: float):
        """Process segment in START state - expecting sync frames (ON, OFF, ON, OFF, ON, OFF)"""
        if not self.start_expecting_off:
            # Looking for ALL_ON
            if seg_type == 'ALL_ON':
                self.start_expecting_off = True
                self.start_sync_count += 1
            # Ignore anything else (like initial ALL_OFF clear)
        else:
            # Looking for ALL_OFF
            if seg_type == 'ALL_OFF':
                self.start_expecting_off = False
                self.start_sync_count += 1
            else:
                # Got something unexpected, reset
                self.start_expecting_off = False
                self.start_sync_count = 0
        
        # After 3 complete sync pairs (6 segments: ON, OFF, ON, OFF, ON, OFF), transition to SEQUENCE
        if self.start_sync_count >= 6:
            self.state = 'SEQUENCE'
            self.current_led_index = 0
            self.leds_since_sync = 0
            self.sync_expecting_off = False  # Next sync starts with ON
    
    def _process_sequence_state(self, seg_type: str, segment: List[LEDFrame], seg_time: float):
        """Process segment in SEQUENCE state - expecting sequential LED frames"""
        if seg_type != 'ONE_LED':
            # Check if it's a sync frame
            if seg_type in ['ALL_OFF', 'ALL_ON']:
                # After every 10 LEDs, we expect a sync
                # Exception: at sequence completion (when all LEDs have been shown or we're at the last LED), 
                # we may have fewer than 10 LEDs in the final batch
                # Check if we're at the end of chain 0 or chain 1
                at_end_of_chain0 = (self.current_led_index >= self.num_leds_chain0 - 1 and 
                                   self.current_led_index < self.num_leds_chain0)
                total_leds = self.num_leds_chain0 + self.num_leds_chain1
                at_end_of_chain1 = (self.current_led_index >= total_leds - 1)
                is_at_end_of_sequence = at_end_of_chain0 or at_end_of_chain1
                
                if self.leds_since_sync != 10 and not is_at_end_of_sequence:
                    self._record_error(f"SEQUENCE seg {self.segment_count} at {seg_time:.3f}s: Expected sync after 10 LEDs but got {self.leds_since_sync} [idx={self.current_led_index}, chain0={self.num_leds_chain0}, at_end_of_chain0={at_end_of_chain0}, is_at_end={is_at_end_of_sequence}]")
                    self.state = 'ERROR'
                    return
                # Transition to SYNC state
                self.state = 'SYNC'
                self._process_sync_state(seg_type, seg_time)
                return
            
            # Not a sync, check if it's an anomaly we can tolerate (UNKNOWN type with few LEDs lit)
            lit_leds = [(i, f) for i, f in enumerate(segment) if f.red > 0 or f.green > 0 or f.blue > 0]
            num_lit = len(lit_leds)
            
            if num_lit == 0:
                # ALL_OFF when expecting ONE_LED
                self._log_all_off_anomaly(seg_time)
                # Continue processing - increment LED index
                self.current_led_index += 1
                self.leds_since_sync += 1
                return
            elif 0 < num_lit <= self.max_lit_leds_tolerance:
                # Few LEDs lit - tolerable anomaly
                self._log_multi_led_anomaly(seg_time, segment, lit_leds)
                # Continue processing - increment LED index
                self.current_led_index += 1
                self.leds_since_sync += 1
                return
            else:
                # Too many LEDs lit - this is an error
                self._record_error(f"SEQUENCE seg {self.segment_count} at {seg_time:.3f}s: Expected ONE_LED or sync but got {num_lit} LEDs lit (threshold: {self.max_lit_leds_tolerance})")
                self.state = 'ERROR'
                return
        
        # Verify the LED index matches expectation (ONE_LED case)
        lit_leds = [i for i, f in enumerate(segment) if f.red > 0 or f.green > 0 or f.blue > 0]
        if len(lit_leds) != 1:
            self._record_error(f"SEQUENCE seg {self.segment_count} at {seg_time:.3f}s: Internal error - ONE_LED classification but {len(lit_leds)} LEDs lit")
            self.state = 'ERROR'
            return
        
        actual_position = lit_leds[0]
        expected_position = self.current_led_index % (self.num_leds_chain0 + self.num_leds_chain1)
        
        # Adjust expected position for chain layout
        if self.current_led_index < self.num_leds_chain0:
            expected_position = self.current_led_index
        else:
            expected_position = self.current_led_index - self.num_leds_chain0
        
        if actual_position != expected_position:
            self._record_error(f"SEQUENCE seg {self.segment_count} at {seg_time:.3f}s: LED position mismatch - expected {expected_position} but got {actual_position} (global index {self.current_led_index})")
            self.state = 'ERROR'
            return
        
        self.current_led_index += 1
        self.leds_since_sync += 1
    
    def _process_sync_state(self, seg_type: str, seg_time: float):
        """Process segment in SYNC state - expecting ALL_ON then ALL_OFF"""
        if not self.sync_expecting_off:
            # Looking for ALL_ON
            if seg_type == 'ALL_ON':
                self.sync_expecting_off = True
            elif seg_type == 'ALL_OFF':
                # Special case: at end of sequence, might go straight to ALL_OFF (restart sequence)
                # This is valid - treat it as completing the sync and resetting
                # Check if we're at the end of either chain
                at_end_of_chain0 = (self.current_led_index >= self.num_leds_chain0 - 1 and 
                                   self.current_led_index < self.num_leds_chain0)
                total_leds = self.num_leds_chain0 + self.num_leds_chain1
                at_end_of_chain1 = (self.current_led_index >= total_leds - 1)
                
                if at_end_of_chain0 or at_end_of_chain1:
                    # At end of sequence - this ALL_OFF is the restart, reset everything
                    self.current_led_index = 0
                    self.state = 'SEQUENCE'
                    self.leds_since_sync = 0
                    self.sync_expecting_off = False
                else:
                    # Not at end - this is an error
                    self._record_error(f"SYNC seg {self.segment_count} at {seg_time:.3f}s: Expected ALL_ON but got {seg_type}")
                    self.state = 'ERROR'
            else:
                # Unexpected input - this is an error in SYNC state
                self._record_error(f"SYNC seg {self.segment_count} at {seg_time:.3f}s: Expected ALL_ON but got {seg_type}")
                self.state = 'ERROR'
        else:
            # Looking for ALL_OFF
            if seg_type == 'ALL_OFF':
                # Complete sync pair (ON+OFF)
                # Check if we've completed the full sequence
                total_leds = self.num_leds_chain0 + self.num_leds_chain1
                if self.current_led_index >= total_leds:
                    # Sequence complete, reset to start
                    self.current_led_index = 0
                
                self.state = 'SEQUENCE'
                self.leds_since_sync = 0
                self.sync_expecting_off = False
            else:
                # Unexpected input - this is an error in SYNC state
                self._record_error(f"SYNC seg {self.segment_count} at {seg_time:.3f}s: Expected ALL_OFF but got {seg_type}")
                self.state = 'ERROR'
    
    def _record_error(self, message: str):
        """Record an error message"""
        self.error_messages.append(message)
    
    def _log_all_off_anomaly(self, seg_time: float):
        """Log when ALL_OFF is encountered during SEQUENCE state"""
        self.anomaly_count += 1
        if len(self.anomaly_details) < self.max_anomaly_logs:
            self.anomaly_details.append({
                'type': 'ALL_OFF',
                'segment': self.segment_count,
                'time': seg_time,
                'expected_index': self.current_led_index,
                'message': f"Seg {self.segment_count} at {seg_time:.3f}s: ALL_OFF when expecting LED {self.current_led_index}"
            })
    
    def _log_multi_led_anomaly(self, seg_time: float, segment: List[LEDFrame], lit_leds: List[Tuple[int, 'LEDFrame']]):
        """Log when multiple LEDs are lit during SEQUENCE state"""
        self.anomaly_count += 1
        if len(self.anomaly_details) < self.max_anomaly_logs:
            # Calculate expected position
            if self.current_led_index < self.num_leds_chain0:
                expected_position = self.current_led_index
            else:
                expected_position = self.current_led_index - self.num_leds_chain0
            
            # Check if expected LED is among lit ones
            expected_led_lit = any(pos == expected_position for pos, _ in lit_leds)
            
            # Get details of lit LEDs (limit to first 10)
            led_details = []
            for i, (pos, frame) in enumerate(lit_leds[:10]):
                led_details.append(f"LED{pos}:RGB({frame.red},{frame.green},{frame.blue})")
            
            detail_str = ", ".join(led_details)
            if len(lit_leds) > 10:
                detail_str += f" +{len(lit_leds)-10} more"
            
            self.anomaly_details.append({
                'type': 'MULTI_LED',
                'segment': self.segment_count,
                'time': seg_time,
                'expected_index': self.current_led_index,
                'expected_position': expected_position,
                'num_lit': len(lit_leds),
                'expected_led_lit': expected_led_lit,
                'led_details': detail_str,
                'message': f"Seg {self.segment_count} at {seg_time:.3f}s: {len(lit_leds)} LEDs lit (expected LED {self.current_led_index} at pos {expected_position}{'✓' if expected_led_lit else '✗'}): {detail_str}"
            })
    
    def get_status(self) -> Dict:
        """Get current state machine status"""
        return {
            'state': self.state,
            'start_sync_count': self.start_sync_count,
            'current_led_index': self.current_led_index,
            'leds_since_sync': self.leds_since_sync,
            'segments_processed': self.segment_count,
            'errors': self.error_messages.copy(),
            'anomaly_count': self.anomaly_count,
            'anomalies': self.anomaly_details.copy()
        }


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
    
    def test_decoder(self, channel: int = 0):
        """Test the state machine decoder and verify color values"""
        print("\n" + "="*80)
        print("TESTING DECODER - Verifying all LEDs have valid colors")
        print("="*80)
        print(f"\nExpected colors: #000000, #282828 (40,40,40), #ffffff (255,255,255)")
        print(f"Any other color indicates a decoding problem\n")
        
        decoder = LEDDecoder(self.timing)
        data_iterator = self.parse_csv_streaming()
        
        prev_point = next(data_iterator, None)
        if prev_point is None:
            print("No data found")
            return
        
        prev_time, prev_ch0, prev_ch1 = prev_point
        prev_level = prev_ch0 if channel == 0 else prev_ch1
        pulse_start_time = prev_time
        
        frame_count = 0
        invalid_colors = []
        
        for current_point in data_iterator:
            time, ch0, ch1 = current_point
            current_level = ch0 if channel == 0 else ch1
            
            # Only process when the channel of interest changes
            if current_level != prev_level:
                duration = time - pulse_start_time
                
                # Feed the transition to the decoder
                frame = decoder.process_transition(pulse_start_time, prev_level, duration)
                
                if frame:
                    frame_count += 1
                    
                    # Check if color is valid (only #000000, #282828, or #ffffff expected)
                    color_hex = f"#{frame.red:02x}{frame.green:02x}{frame.blue:02x}"
                    
                    if not ((frame.red == 0 and frame.green == 0 and frame.blue == 0) or  # #000000
                            (frame.red == 40 and frame.green == 40 and frame.blue == 40) or  # #282828
                            (frame.red == 255 and frame.green == 255 and frame.blue == 255)):  # #ffffff
                        if len(invalid_colors) < 20:
                            invalid_colors.append((frame_count, frame.red, frame.green, frame.blue, color_hex, frame.start_time))
                    
                    if frame_count % 1000 == 0:
                        print(f'\r  Decoded {frame_count} frames...', end='', flush=True)
                
                # Start new pulse
                pulse_start_time = time
                prev_level = current_level
        
        print(f'\r  Decoded {frame_count} frames total')
        print(f"\n{'='*80}")
        
        if invalid_colors:
            print(f"⚠ PROBLEM DETECTED: Found {len(invalid_colors)} frames with invalid colors")
            print(f"\nShowing first {min(len(invalid_colors), 20)} invalid colors:\n")
            for frame_num, r, g, b, hex_color, start_time in invalid_colors:
                print(f"  Frame {frame_num} at t={start_time:.6f}s: R={r:3d} G={g:3d} B={b:3d} ({hex_color})")
            if len(invalid_colors) == 20:
                print(f"  ... (stopped collecting after 20)")
        else:
            print("✓ All frames have valid colors!")
            print("  Decoder is working correctly")
        
        print(f"{'='*80}\n")
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
        Extract pulses from compressed CSV data incrementally for a specific channel
        Yields (start_time, duration, level) tuples ONLY when the specified channel changes
        
        CRITICAL: The CSV format is compressed - it only emits rows when ANY channel changes.
        We must track the previous level for our channel and only yield when it transitions.
        """
        data_iterator = self.parse_csv_streaming()
        
        prev_point = next(data_iterator, None)
        if prev_point is None:
            return
        
        # Initialize with the first point's level for our channel
        time, ch0, ch1 = prev_point
        prev_level = ch0 if channel == 0 else ch1
        prev_time = time
        
        pulse_count = 0
        
        for current_point in data_iterator:
            time, ch0, ch1 = current_point
            current_level = ch0 if channel == 0 else ch1
            
            pulse_count += 1
            if pulse_count % 100000 == 0:
                self.show_progress()
            
            # Only yield when the specified channel actually changes
            if current_level != prev_level:
                duration = time - prev_time
                yield (prev_time, duration, prev_level)
                prev_level = current_level
                prev_time = time
        
        self.show_progress(force=True)
        print()  # New line after progress bar
    
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
        """Analyze the AutoID sequence using state machine while streaming frames"""
        print("\n" + "="*80)
        print("AUTOID SEQUENCE ANALYSIS")
        print("="*80)
        
        # Create decoder state machine for processing frames
        decoder = LEDDecoder(self.timing)
        
        # Create state machine for pattern verification
        state_machine = AutoIDStateMachine(self.num_leds_chain0, self.num_leds_chain1)
        
        current_segment = []
        reset_idx = 0
        total_frames = 0
        first_frame_time = None
        last_frame_time = None
        segment_start_time = None
        
        print("\n  Processing frames and verifying pattern...")
        
        # Process pulses through the decoder state machine
        for start_time, duration, level in self.process_pulses_streaming(channel=0):
            # Feed transitions to decoder (duration is in seconds)
            frame = decoder.process_transition(start_time, level, duration)
            
            if frame is not None:
                total_frames += 1
                
                if first_frame_time is None:
                    first_frame_time = frame.start_time
                    segment_start_time = frame.start_time
                last_frame_time = frame.start_time
                
                # Update timing statistics from the frame
                for bit in frame.bits:
                    if bit.bit_value == 0:
                        self.bit0_high_stats.add(bit.high_time)
                        self.bit0_low_stats.add(bit.low_time)
                    else:
                        self.bit1_high_stats.add(bit.high_time)
                        self.bit1_low_stats.add(bit.low_time)
                    
                    if not bit.is_valid:
                        self.total_errors += 1
                        self.timing_errors.append({
                            'time': bit.start_time,
                            'high_us': bit.high_time,
                            'low_us': bit.low_time,
                            'bit_value': bit.bit_value,
                            'error': bit.error_msg
                        })
                
                # Check if there's a reset before this frame
                while reset_idx < len(self.reset_periods) and self.reset_periods[reset_idx][0] < frame.start_time:
                    if current_segment:
                        # Process complete segment with state machine
                        state_machine.process_segment(current_segment, segment_start_time)
                        
                        # Show first few segments for debugging
                        if state_machine.segment_count <= 20:
                            self._display_segment(state_machine.segment_count, current_segment, segment_start_time)
                        
                        current_segment = []
                        segment_start_time = frame.start_time
                    reset_idx += 1
                
                current_segment.append(frame)
                
                # Progress indication
                if total_frames % 5000 == 0:
                    print(f'\r  Processed {total_frames} frames, {state_machine.segment_count} segments...', end='', flush=True)
        
        # Process final segment
        if current_segment:
            state_machine.process_segment(current_segment, segment_start_time)
            if state_machine.segment_count <= 20:
                self._display_segment(state_machine.segment_count, current_segment, segment_start_time)
        
        print()  # New line after progress
        
        # Display state machine results
        self._display_autoid_results(state_machine, total_frames, first_frame_time, last_frame_time)
    
    def _display_segment(self, seg_num: int, segment: List[LEDFrame], seg_time: float):
        """Display segment information for debugging"""
        print(f"\n--- Segment {seg_num} at {seg_time:.3f}s ({len(segment)} LEDs) ---")
        
        # Classify segment
        all_off = all(f.red == 0 and f.green == 0 and f.blue == 0 for f in segment)
        if all_off:
            print(f"  → ALL OFF")
            return
        
        first_color = (segment[0].red, segment[0].green, segment[0].blue)
        all_same = all((f.red, f.green, f.blue) == first_color for f in segment)
        if all_same and first_color != (0, 0, 0):
            print(f"  → ALL ON: RGB({first_color[0]},{first_color[1]},{first_color[2]})")
            return
        
        # Check for single LED
        lit_leds = [(i, f) for i, f in enumerate(segment) if f.red > 0 or f.green > 0 or f.blue > 0]
        if len(lit_leds) == 1:
            i, frame = lit_leds[0]
            print(f"  → ONE LED at position {i}: RGB({frame.red},{frame.green},{frame.blue})")
        else:
            print(f"  → MULTIPLE LEDS: {len(lit_leds)} LEDs lit")
            for i, frame in lit_leds[:5]:
                print(f"     LED {i}: RGB({frame.red},{frame.green},{frame.blue})")
    
    def _display_autoid_results(self, state_machine: AutoIDStateMachine, total_frames: int, 
                                first_frame_time: Optional[float], last_frame_time: Optional[float]):
        """Display AutoID pattern verification results"""
        status = state_machine.get_status()
        
        print(f"\n{'='*80}")
        print(f"AUTOID PATTERN VERIFICATION RESULTS")
        print(f"{'='*80}")
        
        print(f"\nTotal frames decoded: {total_frames}")
        print(f"Total segments processed: {status['segments_processed']}")
        
        if first_frame_time and last_frame_time:
            duration = last_frame_time - first_frame_time
            print(f"Recording duration: {duration:.2f} seconds")
        
        print(f"\nState Machine Status:")
        print(f"  Final state: {status['state']}")
        print(f"  Start sync count: {status['start_sync_count']}")
        print(f"  Current LED index: {status['current_led_index']}")
        print(f"  LEDs since last sync: {status['leds_since_sync']}")
        
        # Display anomalies if any
        if status['anomaly_count'] > 0:
            print(f"\n{'~'*80}")
            print(f"ANOMALIES DETECTED: {status['anomaly_count']} (tolerated, processing continued)")
            print(f"{'~'*80}")
            for i, anomaly in enumerate(status['anomalies'], 1):
                print(f"{i}. {anomaly['message']}")
            if status['anomaly_count'] > len(status['anomalies']):
                print(f"\n... and {status['anomaly_count'] - len(status['anomalies'])} more anomalies (not logged)")
        
        if status['errors']:
            print(f"\n{'!'*80}")
            print(f"ERRORS DETECTED: {len(status['errors'])}")
            print(f"{'!'*80}")
            for i, error in enumerate(status['errors'][:20], 1):
                print(f"{i}. {error}")
            if len(status['errors']) > 20:
                print(f"\n... and {len(status['errors']) - 20} more errors")
        else:
            if status['anomaly_count'] > 0:
                print(f"\n{'='*80}")
                print(f"✓ PATTERN VALIDATION SUCCESSFUL (with {status['anomaly_count']} anomalies)")
                print(f"{'='*80}")
                print(f"AutoID sequence is correct but has minor anomalies")
            else:
                print(f"\n{'='*80}")
                print(f"✓ PATTERN VALIDATION SUCCESSFUL")
                print(f"{'='*80}")
                print(f"AutoID sequence is correct!")
    
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
    
    parser.add_argument(
        '--test-decoder',
        action='store_true',
        help='Test the state machine decoder and verify it only produces valid colors (#000000, #282828, #ffffff)'
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
    
    if args.test_decoder:
        # Test mode: verify state machine only produces valid colors
        print("\n=== Testing State Machine Decoder ===")
        print(f"Expected colors: #000000 (off), #282828 (RGB 40,40,40), #ffffff (white)")
        print(f"Processing channel 0...\n")
        analyzer.test_decoder(channel=0)
    else:
        analyzer.analyze()


if __name__ == '__main__':
    main()
