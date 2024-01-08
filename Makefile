ROOTDIR = $(realpath $(CURDIR)/..)
WORKING_DIR = $(realpath $(CURDIR))
DOCKER_ESP_IDF ?= docker run --rm -v $(ROOTDIR):$(ROOTDIR) -w $(WORKING_DIR) espressif/idf:v5.1.2
CMD ?= ./build.sh

all: buildall

buildall: Makefile $(wildcard ./*) $(wildcard components/core/*) $(wildcard components/comms/*)
	$(DOCKER_ESP_IDF) $(CMD)

clean:
	$(DOCKER_ESP_IDF) rm -rf build sdkconfig || true

PORT ?= COM3

flashwsl: buildall
	python.exe -m esptool -p $(PORT) -b 460800 --before default_reset --after hard_reset --chip esp32  write_flash --flash_mode dio --flash_size 4MB --flash_freq 40m 0x1000 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0xe000 build/ota_data_initial.bin 0x10000 build/test_raft_core.bin
	python.exe ../scripts/SerialMonitor.py $(PORT) -g
