#!/usr/bin/env python3
"""
Binary Size Analyzer for ESP-IDF Projects
Analyzes the build output to determine which components and sub-modules
are consuming the most space in the binary.
"""

import os
import sys
import re
import json
import subprocess
from pathlib import Path
from collections import defaultdict, namedtuple
import argparse

# Data structures
ComponentSize = namedtuple('ComponentSize', ['name', 'text_size', 'data_size', 'bss_size', 'total_size'])
ModuleSize = namedtuple('ModuleSize', ['name', 'component', 'text_size', 'data_size', 'bss_size', 'total_size'])

class BinarySizeAnalyzer:
    def __init__(self, build_dir, toolchain_prefix="xtensa-esp32-elf-"):
        self.build_dir = Path(build_dir)
        self.toolchain_prefix = toolchain_prefix
        self.component_sizes = {}
        self.module_sizes = {}
        self.total_sizes = {'text': 0, 'data': 0, 'bss': 0}
        
    def find_toolchain_size(self):
        """Find the size tool in the toolchain"""
        # Common locations for ESP32 toolchain
        possible_paths = [
            f"/home/rob/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin/{self.toolchain_prefix}size",
            f"/home/rob/.espressif/tools/xtensa-esp32-elf/esp-2022r1-11.2.0/xtensa-esp32-elf/bin/{self.toolchain_prefix}size",
            f"/home/rob/.espressif/tools/xtensa-esp32-elf/esp-2021r2-patch5-8.4.0/xtensa-esp32-elf/bin/{self.toolchain_prefix}size",
            f"{self.toolchain_prefix}size"  # If in PATH
        ]
        
        for path in possible_paths:
            if os.path.exists(path) or subprocess.run(['which', path.split('/')[-1]], 
                                                     capture_output=True).returncode == 0:
                return path
                
        raise FileNotFoundError(f"Could not find {self.toolchain_prefix}size tool")
    
    def get_size_info(self, elf_file):
        """Get size information from ELF file using size tool"""
        size_tool = self.find_toolchain_size()
        
        try:
            # Use -A flag for detailed section information
            result = subprocess.run([size_tool, '-A', str(elf_file)], 
                                  capture_output=True, text=True, check=True)
            return result.stdout
        except subprocess.CalledProcessError as e:
            print(f"Error running size tool: {e}")
            return None
    
    def parse_size_output(self, size_output):
        """Parse the output from the size tool"""
        sections = {}
        
        for line in size_output.split('\n'):
            line = line.strip()
            if not line or line.startswith('section') or line.startswith('/'):
                continue
                
            parts = line.split()
            if len(parts) >= 2:
                section_name = parts[0]
                try:
                    size = int(parts[1])
                    sections[section_name] = size
                except ValueError:
                    continue
                    
        return sections
    
    def categorize_sections(self, sections):
        """Categorize sections into text, data, and bss"""
        text_sections = ['.flash.text', '.iram0.text', '.iram0.vectors']
        data_sections = ['.flash.rodata', '.dram0.data', '.iram0.data']
        bss_sections = ['.dram0.bss', '.iram0.bss', '.ext_ram.bss', '.noinit']
        
        text_size = sum(sections.get(sec, 0) for sec in text_sections)
        data_size = sum(sections.get(sec, 0) for sec in data_sections)
        bss_size = sum(sections.get(sec, 0) for sec in bss_sections)
        
        # Add any other sections that contain code or data
        for sec_name, size in sections.items():
            if any(keyword in sec_name for keyword in ['.text', 'code', 'CODE']):
                text_size += size
            elif any(keyword in sec_name for keyword in ['.rodata', '.data', 'DATA']):
                data_size += size
            elif any(keyword in sec_name for keyword in ['.bss', 'BSS']):
                bss_size += size
                
        return text_size, data_size, bss_size
    
    def analyze_libraries(self):
        """Analyze individual library files"""
        esp_idf_dir = self.build_dir / "esp-idf"
        
        if not esp_idf_dir.exists():
            print(f"ESP-IDF build directory not found: {esp_idf_dir}")
            return
            
        print("Analyzing component libraries...")
        
        for component_dir in esp_idf_dir.iterdir():
            if component_dir.is_dir():
                lib_files = list(component_dir.glob("*.a"))
                
                for lib_file in lib_files:
                    component_name = component_dir.name
                    lib_size = self.analyze_library_contents(lib_file, component_name)
                    
                    if lib_size:
                        self.component_sizes[component_name] = lib_size
    
    def analyze_library_contents(self, lib_file, component_name):
        """Analyze contents of a static library"""
        try:
            # Extract object files from library
            result = subprocess.run(['ar', 't', str(lib_file)], 
                                  capture_output=True, text=True, check=True)
            
            object_files = [f.strip() for f in result.stdout.split('\n') if f.strip()]
            
            total_text = 0
            total_data = 0
            total_bss = 0
            
            # Analyze each object file
            for obj_file in object_files:
                obj_size = self.get_object_size_from_library(lib_file, obj_file)
                if obj_size:
                    total_text += obj_size[0]
                    total_data += obj_size[1]
                    total_bss += obj_size[2]
                    
                    # Store individual module info
                    module_name = obj_file.replace('.c.obj', '').replace('.cpp.obj', '').replace('.S.obj', '')
                    self.module_sizes[f"{component_name}/{module_name}"] = ModuleSize(
                        module_name, component_name, obj_size[0], obj_size[1], obj_size[2], sum(obj_size)
                    )
            
            total_size = total_text + total_data + total_bss
            return ComponentSize(component_name, total_text, total_data, total_bss, total_size)
            
        except subprocess.CalledProcessError:
            # Try to get library size directly
            return self.get_library_size_direct(lib_file, component_name)
    
    def get_object_size_from_library(self, lib_file, obj_file):
        """Get size of specific object file from library"""
        try:
            # Extract object file temporarily
            temp_dir = Path("/tmp/esp_analysis")
            temp_dir.mkdir(exist_ok=True)
            
            subprocess.run(['ar', 'x', str(lib_file), obj_file], 
                          cwd=temp_dir, check=True)
            
            obj_path = temp_dir / obj_file
            if obj_path.exists():
                size_output = self.get_size_info(obj_path)
                if size_output:
                    sections = self.parse_size_output(size_output)
                    text, data, bss = self.categorize_sections(sections)
                    
                    # Clean up
                    obj_path.unlink()
                    return (text, data, bss)
                    
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass
            
        return None
    
    def get_library_size_direct(self, lib_file, component_name):
        """Get library size using nm tool if available"""
        try:
            size_tool = self.find_toolchain_size()
            nm_tool = size_tool.replace('size', 'nm')
            
            result = subprocess.run([nm_tool, '--print-size', '--size-sort', str(lib_file)], 
                                  capture_output=True, text=True, check=True)
            
            total_size = 0
            for line in result.stdout.split('\n'):
                if line.strip():
                    parts = line.split()
                    if len(parts) >= 2:
                        try:
                            size = int(parts[1], 16)  # Size is in hex
                            total_size += size
                        except ValueError:
                            continue
            
            # Rough estimation: assume 70% text, 20% data, 10% bss
            text_size = int(total_size * 0.7)
            data_size = int(total_size * 0.2)
            bss_size = int(total_size * 0.1)
            
            return ComponentSize(component_name, text_size, data_size, bss_size, total_size)
            
        except (subprocess.CalledProcessError, FileNotFoundError):
            # Fallback: use file size
            file_size = lib_file.stat().st_size
            # Very rough estimation
            text_size = int(file_size * 0.6)
            data_size = int(file_size * 0.3)
            bss_size = int(file_size * 0.1)
            
            return ComponentSize(component_name, text_size, data_size, bss_size, file_size)
    
    def analyze_map_file(self):
        """Analyze the linker map file for detailed size information"""
        map_files = list(self.build_dir.glob("*.map"))
        
        if not map_files:
            print("No map file found")
            return
            
        map_file = map_files[0]
        print(f"Analyzing map file: {map_file}")
        
        with open(map_file, 'r') as f:
            content = f.read()
            
        # Parse memory usage from map file
        self.parse_map_file_memory_usage(content)
        self.parse_map_file_symbols(content)
    
    def parse_map_file_memory_usage(self, content):
        """Parse memory usage from map file"""
        # Look for memory configuration section
        memory_pattern = r'Memory Configuration.*?(?=\n\n|\nLinker script)'
        memory_match = re.search(memory_pattern, content, re.DOTALL)
        
        if memory_match:
            print("Memory Configuration found in map file")
            
        # Look for section sizes
        section_pattern = r'(\.[a-zA-Z0-9_.]+)\s+0x[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)'
        sections = re.findall(section_pattern, content)
        
        for section_name, size_hex in sections:
            size = int(size_hex, 16)
            if size > 1000:  # Only show sections > 1KB
                print(f"Section {section_name}: {size:,} bytes")
    
    def parse_map_file_symbols(self, content):
        """Parse symbol information from map file"""
        # Look for largest symbols
        symbol_pattern = r'0x[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)\s+([^\s]+)'
        symbols = re.findall(symbol_pattern, content)
        
        large_symbols = []
        for size_hex, symbol in symbols:
            size = int(size_hex, 16)
            if size > 1000:  # Only symbols > 1KB
                large_symbols.append((symbol, size))
        
        # Sort by size
        large_symbols.sort(key=lambda x: x[1], reverse=True)
        
        print("\nLargest symbols (> 1KB):")
        for symbol, size in large_symbols[:20]:  # Top 20
            print(f"  {symbol}: {size:,} bytes")
    
    def generate_report(self):
        """Generate comprehensive size analysis report"""
        print("\n" + "="*80)
        print("ESP-IDF BINARY SIZE ANALYSIS REPORT")
        print("="*80)
        
        # Analyze main ELF file
        elf_files = list(self.build_dir.glob("*.elf"))
        if elf_files:
            main_elf = elf_files[0]
            print(f"\nMain ELF file: {main_elf.name}")
            print(f"File size: {main_elf.stat().st_size:,} bytes")
            
            size_output = self.get_size_info(main_elf)
            if size_output:
                print("\nSection breakdown:")
                print(size_output)
        
        # Analyze libraries
        self.analyze_libraries()
        
        # Sort components by size
        sorted_components = sorted(self.component_sizes.values(), 
                                 key=lambda x: x.total_size, reverse=True)
        
        print(f"\nCOMPONENT SIZE BREAKDOWN (Top 20):")
        print("-" * 80)
        print(f"{'Component':<25} {'Text':<12} {'Data':<12} {'BSS':<12} {'Total':<12}")
        print("-" * 80)
        
        for comp in sorted_components[:20]:
            print(f"{comp.name:<25} {comp.text_size:>10,}  {comp.data_size:>10,}  "
                  f"{comp.bss_size:>10,}  {comp.total_size:>10,}")
        
        # Analyze RaftCore modules specifically
        raft_modules = {k: v for k, v in self.module_sizes.items() if 'RaftCore' in k}
        if raft_modules:
            print(f"\nRAFTCORE MODULE BREAKDOWN:")
            print("-" * 80)
            sorted_raft = sorted(raft_modules.values(), key=lambda x: x.total_size, reverse=True)
            
            for module in sorted_raft[:15]:  # Top 15 RaftCore modules
                print(f"{module.name:<35} {module.total_size:>10,} bytes")
        
        # Analyze map file
        self.analyze_map_file()
        
        # Recommendations
        self.generate_recommendations()
    
    def generate_recommendations(self):
        """Generate recommendations for size reduction"""
        print(f"\nSIZE REDUCTION RECOMMENDATIONS:")
        print("="*80)
        
        # Find largest components
        sorted_components = sorted(self.component_sizes.values(), 
                                 key=lambda x: x.total_size, reverse=True)
        
        large_components = [comp for comp in sorted_components if comp.total_size > 100000]  # > 100KB
        
        for comp in large_components[:5]:  # Top 5 largest
            print(f"\n{comp.name}: {comp.total_size:,} bytes")
            
            if comp.name == "RaftCore":
                print("  - Consider disabling unused RaftCore features")
                print("  - Check if all communication protocols are needed")
                print("  - Disable unused device drivers")
                print("  - Review file system and networking features")
                
            elif comp.name == "bt":
                print("  - Consider disabling Bluetooth if not needed")
                print("  - Reduce Bluetooth buffer sizes")
                print("  - Disable unused Bluetooth services")
                
            elif comp.name == "lwip":
                print("  - Reduce TCP/UDP buffer sizes")
                print("  - Disable IPv6 if not needed")
                print("  - Disable unused networking features")
                
            elif comp.name == "mbedtls":
                print("  - Disable unused crypto algorithms")
                print("  - Reduce certificate bundle size")
                print("  - Use hardware crypto acceleration")
                
            elif "Scader" in comp.name:
                print("  - Remove unused Scader modules from main.cpp")
                print("  - Disable unnecessary features in configuration")

def main():
    parser = argparse.ArgumentParser(description='Analyze ESP-IDF binary size')
    parser.add_argument('--build-dir', default='build/ScaderRelays',
                       help='Build directory (default: build/ScaderRelays)')
    parser.add_argument('--toolchain', default='xtensa-esp32-elf-',
                       help='Toolchain prefix (default: xtensa-esp32-elf-)')
    
    args = parser.parse_args()
    
    # Get script directory and project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    build_dir = project_root / args.build_dir
    
    if not build_dir.exists():
        print(f"Error: Build directory not found: {build_dir}")
        sys.exit(1)
    
    analyzer = BinarySizeAnalyzer(build_dir, args.toolchain)
    analyzer.generate_report()

if __name__ == "__main__":
    main()
