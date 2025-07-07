#!/usr/bin/env python3
"""
RaftCore Module Analyzer
Specifically analyzes RaftCore components to identify what can be safely removed
to reduce binary size for ScaderRelays build.
"""

import os
import re
import json
from pathlib import Path
from collections import defaultdict, Counter

class RaftCoreAnalyzer:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.raftcore_dir = self.find_raftcore_directory()
        self.usage_map = defaultdict(set)  # module -> set of files that use it
        self.dependency_map = defaultdict(set)  # module -> set of modules it depends on
        self.size_estimates = {}  # module -> estimated size
        
    def find_raftcore_directory(self):
        """Find RaftCore source directory"""
        possible_paths = [
            self.project_root / "build" / "ScaderRelays" / "raft" / "RaftCore",
            self.project_root / "components" / "RaftCore",
            self.project_root / "raft" / "RaftCore"
        ]
        
        for path in possible_paths:
            if path.exists():
                return path
                
        print("Warning: RaftCore directory not found")
        return None
    
    def scan_raftcore_modules(self):
        """Scan RaftCore directory structure to identify modules"""
        if not self.raftcore_dir:
            return {}
            
        modules = {}
        
        # Look for component directories
        components_dir = self.raftcore_dir / "components"
        if components_dir.exists():
            for category_dir in components_dir.iterdir():
                if category_dir.is_dir():
                    category = category_dir.name
                    modules[category] = []
                    
                    for module_dir in category_dir.iterdir():
                        if module_dir.is_dir():
                            module_info = self.analyze_module(module_dir, category)
                            if module_info:
                                modules[category].append(module_info)
        
        return modules
    
    def analyze_module(self, module_dir, category):
        """Analyze a specific RaftCore module"""
        module_name = module_dir.name
        
        # Count source files
        cpp_files = list(module_dir.glob("*.cpp"))
        c_files = list(module_dir.glob("*.c"))
        h_files = list(module_dir.glob("*.h"))
        hpp_files = list(module_dir.glob("*.hpp"))
        
        total_lines = 0
        file_sizes = 0
        
        for source_file in cpp_files + c_files:
            try:
                with open(source_file, 'r', encoding='utf-8', errors='ignore') as f:
                    lines = len(f.readlines())
                    total_lines += lines
                    file_sizes += source_file.stat().st_size
            except:
                pass
        
        # Estimate binary size based on source size (very rough)
        estimated_size = file_sizes * 0.8  # Rough compilation ratio
        
        return {
            'name': module_name,
            'category': category,
            'cpp_files': len(cpp_files),
            'c_files': len(c_files),
            'header_files': len(h_files + hpp_files),
            'total_lines': total_lines,
            'source_size': file_sizes,
            'estimated_binary_size': estimated_size,
            'path': str(module_dir)
        }
    
    def scan_project_usage(self):
        """Scan project files to see which RaftCore modules are actually used"""
        usage_patterns = {
            # Communication modules
            'CommsChannels': [r'#include.*CommsChannel', r'CommsChannelManager', r'CommsChannel[^M]'],
            'ProtocolRICJSON': [r'#include.*ProtocolRICJSON', r'ProtocolRICJSON'],
            'ProtocolRICSerial': [r'#include.*ProtocolRICSerial', r'ProtocolRICSerial'],
            'ProtocolRawMsg': [r'#include.*ProtocolRawMsg', r'ProtocolRawMsg'],
            'ProtocolOverAscii': [r'#include.*ProtocolOverAscii', r'ProtocolOverAscii'],
            'FileStreamProtocols': [r'#include.*FileStream', r'FileUpload', r'FileDownload'],
            'ROSSerial': [r'#include.*ROSSerial', r'ROSSerialMsg'],
            
            # Core modules
            'NetworkSystem': [r'#include.*NetworkSystem', r'NetworkSystem', r'WiFiScanner'],
            'FileSystem': [r'#include.*FileSystem', r'FileSystem[^C]'],
            'MQTT': [r'#include.*MQTT', r'MQTTProtocol', r'RaftMQTTClient'],
            'LEDPixels': [r'#include.*LEDPixels', r'LEDPixels', r'ESP32RMTLedStrip'],
            'DeviceManager': [r'#include.*DeviceManager', r'DeviceFactory'],
            'Bus': [r'#include.*Bus', r'BusAddrStatus', r'BusSerial', r'RaftBusSystem'],
            'Logger': [r'#include.*Logger', r'LoggerCore'],
            'ArduinoUtils': [r'#include.*Arduino', r'ArduinoGPIO', r'ArduinoWString'],
            'StatusIndicator': [r'#include.*StatusIndicator', r'StatusIndicator'],
            'SupervisorStats': [r'#include.*SupervisorStats', r'SupervisorStats'],
            'RestAPIEndpoints': [r'#include.*RestAPI', r'RestAPIEndpointManager'],
            'DebugGlobals': [r'#include.*DebugGlobals', r'DebugGlobals'],
            'Utils': [r'#include.*Utils', r'RaftUtils', r'PlatformUtils'],
            'ExpressionEval': [r'#include.*Expression', r'ExpressionEval', r'tinyexpr'],
            'DNSResolver': [r'#include.*DNSResolver', r'DNSResolver'],
            'ESPMDNS': [r'#include.*mdns', r'mdns_'],
            'NamedValueProvider': [r'#include.*NamedValue', r'NamedValueProvider'],
            'MiniHDLC': [r'#include.*MiniHDLC', r'MiniHDLC'],
            'ConfigPinMap': [r'#include.*ConfigPinMap', r'ConfigPinMap'],
            'DebounceButton': [r'#include.*DebounceButton', r'DebounceButton'],
        }
        
        project_files = []
        
        # Scan main directory
        main_dir = self.project_root / "main"
        if main_dir.exists():
            project_files.extend(main_dir.glob("*.cpp"))
            project_files.extend(main_dir.glob("*.c"))
            project_files.extend(main_dir.glob("*.h"))
        
        # Scan components directory
        components_dir = self.project_root / "components"
        if components_dir.exists():
            for subdir in components_dir.rglob("*"):
                if subdir.is_file() and subdir.suffix in ['.cpp', '.c', '.h', '.hpp']:
                    project_files.append(subdir)
        
        print(f"Scanning {len(project_files)} project files for RaftCore usage...")
        
        for pattern_name, patterns in usage_patterns.items():
            for file_path in project_files:
                try:
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                        
                    for pattern in patterns:
                        if re.search(pattern, content, re.IGNORECASE):
                            self.usage_map[pattern_name].add(str(file_path))
                            break
                            
                except Exception as e:
                    pass
    
    def analyze_cmake_dependencies(self):
        """Analyze CMakeLists.txt files to understand dependencies"""
        cmake_files = list(self.project_root.rglob("CMakeLists.txt"))
        
        raftcore_deps = set()
        
        for cmake_file in cmake_files:
            try:
                with open(cmake_file, 'r') as f:
                    content = f.read()
                
                # Look for RaftCore requirements
                if 'RaftCore' in content:
                    requires_match = re.search(r'REQUIRES[^)]*RaftCore[^)]*', content)
                    if requires_match:
                        # Extract other components in the same REQUIRES clause
                        requires_text = requires_match.group(0)
                        components = re.findall(r'\b[A-Za-z][A-Za-z0-9_]*\b', requires_text)
                        raftcore_deps.update(components)
                        
            except Exception:
                pass
        
        return raftcore_deps
    
    def generate_removal_recommendations(self):
        """Generate recommendations for what can be safely removed"""
        print("\n" + "="*80)
        print("RAFTCORE MODULE USAGE ANALYSIS")
        print("="*80)
        
        modules = self.scan_raftcore_modules()
        self.scan_project_usage()
        
        unused_modules = []
        used_modules = []
        
        print(f"\nModules found in project files:")
        for module, files in self.usage_map.items():
            if files:
                used_modules.append(module)
                print(f"  ✓ {module}: used in {len(files)} files")
        
        # Check which RaftCore categories might be unused
        all_possible_modules = [
            'CommsChannels', 'ProtocolRICJSON', 'ProtocolRICSerial', 'ProtocolRawMsg',
            'ProtocolOverAscii', 'FileStreamProtocols', 'ROSSerial', 'MQTT',
            'LEDPixels', 'ExpressionEval', 'DNSResolver', 'ESPMDNS', 'MiniHDLC'
        ]
        
        print(f"\nPotentially unused modules:")
        for module in all_possible_modules:
            if module not in used_modules:
                unused_modules.append(module)
                print(f"  ✗ {module}: not detected in project")
        
        print(f"\nRECOMMENDATIONS FOR SIZE REDUCTION:")
        print("="*50)
        
        if unused_modules:
            print("\n1. MODULES THAT CAN LIKELY BE DISABLED:")
            for module in unused_modules:
                if module in ['ROSSerial', 'ESPMDNS', 'ExpressionEval', 'MiniHDLC']:
                    print(f"   - {module}: Specialized functionality likely not needed for ScaderRelays")
                elif module in ['FileStreamProtocols']:
                    print(f"   - {module}: File upload/download not needed for basic relay control")
                elif module in ['ProtocolRICSerial', 'ProtocolRawMsg']:
                    print(f"   - {module}: Alternative communication protocols")
        
        print("\n2. CONFIGURATION OPTIONS TO REDUCE RAFTCORE SIZE:")
        print("   Add these to your CMakeLists.txt or component configuration:")
        print("   - Disable unused communication protocols")
        print("   - Disable file system features if not needed")
        print("   - Disable MQTT if using only local control")
        print("   - Disable LED pixel support if not using addressable LEDs")
        
        print("\n3. SCADER MODULES TO REMOVE (from main.cpp):")
        print("   Based on ScaderRelays use case, consider removing:")
        print("   - ScaderRFID (unless using RFID)")
        print("   - ScaderElecMeters (unless monitoring power)")
        print("   - ScaderLocks (unless controlling locks)")
        print("   - ScaderLEDPixels (unless using addressable LEDs)")
        print("   - ScaderPulseCounter (unless counting pulses)")
        print("   - ScaderBTHome (unless using Bluetooth)")
        print("   - ScaderShades (unless controlling window shades)")
        print("   - ScaderOpener (unless controlling door openers)")
        
        # Analyze actual module sizes if available
        if modules:
            print(f"\n4. RAFTCORE MODULE SIZE ESTIMATES:")
            all_modules = []
            for category, module_list in modules.items():
                all_modules.extend(module_list)
            
            # Sort by estimated size
            all_modules.sort(key=lambda x: x['estimated_binary_size'], reverse=True)
            
            print(f"   Largest RaftCore modules by estimated size:")
            for module in all_modules[:10]:
                size_kb = module['estimated_binary_size'] / 1024
                print(f"   - {module['category']}/{module['name']}: ~{size_kb:.1f} KB "
                      f"({module['total_lines']} lines)")
    
    def check_specific_scader_relays_needs(self):
        """Check what ScaderRelays specifically needs"""
        print(f"\n5. SCADERRELAYS SPECIFIC ANALYSIS:")
        print("="*50)
        
        # Check what ScaderRelays.cpp actually uses
        scader_relays_file = None
        for path in self.project_root.rglob("ScaderRelays.cpp"):
            scader_relays_file = path
            break
        
        if scader_relays_file:
            print(f"   Analyzing {scader_relays_file}...")
            
            try:
                with open(scader_relays_file, 'r') as f:
                    content = f.read()
                
                # Check for specific RaftCore features used
                features_used = []
                
                if 'RestAPIEndpointManager' in content:
                    features_used.append("REST API endpoints - NEEDED")
                if 'NetworkSystem' in content:
                    features_used.append("Network system - NEEDED")
                if 'CommsChannelMsg' in content:
                    features_used.append("Communications - NEEDED")
                if 'ScaderCommon' in content:
                    features_used.append("Scader common functionality - NEEDED")
                if 'ConfigPinMap' in content:
                    features_used.append("Pin configuration - NEEDED")
                if 'SysManager' in content:
                    features_used.append("System manager - NEEDED")
                if 'RaftUtils' in content:
                    features_used.append("Raft utilities - NEEDED")
                if 'SPIDimmer' in content:
                    features_used.append("SPI dimmer control - NEEDED for relays")
                
                print("   Features actually used by ScaderRelays:")
                for feature in features_used:
                    print(f"     ✓ {feature}")
                
                # Features likely NOT needed
                not_needed = [
                    "MQTT client (unless remote monitoring needed)",
                    "File system (unless storing config files)",
                    "LED pixels (unless status LEDs needed)",
                    "Device manager (unless managing I2C devices)",
                    "Expression evaluator (unless doing calculations)",
                    "DNS resolver (unless using hostnames)",
                    "mDNS (unless using service discovery)",
                    "ROS Serial (definitely not needed)",
                    "File upload/download protocols (not needed)",
                ]
                
                print("\n   Features likely NOT needed by ScaderRelays:")
                for feature in not_needed:
                    print(f"     ✗ {feature}")
                    
            except Exception as e:
                print(f"   Error reading ScaderRelays.cpp: {e}")

def main():
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    
    analyzer = RaftCoreAnalyzer(project_root)
    analyzer.generate_removal_recommendations()
    analyzer.check_specific_scader_relays_needs()

if __name__ == "__main__":
    main()
