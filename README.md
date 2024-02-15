# ScaderESP32 - Firmware for LightScader and other Home Automation

## Building

Two build methods are available:
- A Makefile which uses docker and the Espressif ESP IDF builder image - look inside the Makefile to identify the version of ESP IDF used
- A shell script which uses a locally installed ESP IDF which must be in the ~/esp folder - look inside the script for the folder which may depend on ESP IDF version

### Makefile build

To build with the Makefile:
- make clean
- make

Should build a fresh image. And to build and also flash using WSL - and then open a serial monitor in the same terminal window
- make flashwsl PORT=COM???
- OR
- make flashwsl PORT=/dev/ttyUSB???

### Build script build
To build with the build.sh script:
- chmod +x ./build.sh
- ./build.sh

This will build and then flash and then open the serial monitor in the terminal window


