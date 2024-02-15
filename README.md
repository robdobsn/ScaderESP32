# ScaderESP32 - Firmware for LightScader and other Home Automation

## Building

Two build methods are available:
- A Makefile which uses docker and the Espressif ESP IDF builder image - look inside the Makefile to identify the version of ESP IDF used
- A shell script which uses a locally installed ESP IDF which must be in the ~/esp folder - look inside the script for the folder which may depend on ESP IDF version

### Makefile build

To build with the Makefile:

```
make
```

Build, flash and monitor with this:

```
make flash PORT=<your-serial-port>
```

Where \<your-serial-port> is COMxxxx on Windows or /dev/ttyxxxx on Mac/Linux

And clean in case you want a full rebuild with:

```
make clean
```