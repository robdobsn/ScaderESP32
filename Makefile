# Build Project
BUILD_CONFIG_NAME ?= LightScader
DOCKER ?= 1
WSL ?= 1
MONITOR ?= SerialMonitor.py
BUILD_BASE_FOLDER ?= build
BUILD_RAFT_ARTEFACTS_DIR ?= build_raft_artefacts
BUILD_DIR = $(BUILD_BASE_FOLDER)/$(BUILD_CONFIG_NAME)
BUILD_CONFIG_BASE_DIR = systypes
BUILD_CONFIG_DIR = $(BUILD_CONFIG_BASE_DIR)/$(BUILD_CONFIG_NAME)
ROOTDIR = $(realpath $(CURDIR))
# DOCKER_EXEC ?= docker run --rm -v $(ROOTDIR):/project -w /project espressif/idf:v5.1.2
DOCKER_EXEC = docker build -t sandbot7builder . && docker run --rm -v $(ROOTDIR):/project -w /project sandbot7builder
LOCAL_EXEC = . ~/esp/esp-idf-v5.1.2/export.sh
CMD ?= idf.py -B $(BUILD_DIR) build
PORT ?= COM9

ifeq ($(WSL),1)
	PYTHON_FOR_FLASH_MONITOR ?= python.exe
else
	PYTHON_FOR_FLASH_MONITOR ?= python3
endif

# Custom command to delete sdkconfig if sdkconfig.defaults is newer
define DELETE_SDKCONFIG_IF_OUTDATED
    if [ -f "$(BUILD_CONFIG_DIR)/sdkconfig.defaults" -a -f "$(BUILD_CONFIG_DIR)/sdkconfig" ]; then \
        if [ "$(BUILD_CONFIG_DIR)/sdkconfig.defaults" -nt "$(BUILD_CONFIG_DIR)/sdkconfig" ]; then \
            echo "-------------------- Deleting sdkconfig --------------------"; \
            rm -f "$(BUILD_CONFIG_DIR)/sdkconfig"; \
        fi; \
    fi
endef

all: build

ifeq ($(DOCKER),1)
build: Makefile $(wildcard main/*) $(wildcard components/*) $(wildcard $(BUILD_CONFIG_DIR)/*)
	@echo "-------------------- Rebuilding in Docker --------------------"
	@$(DELETE_SDKCONFIG_IF_OUTDATED)
	$(DOCKER_EXEC) $(CMD)
clean:
	$(DOCKER_EXEC) rm -rf ./build/ ./build_raft_artefacts/ $(BUILD_CONFIG_DIR)/sdkconfig || true
else
build: Makefile $(wildcard main/*) $(wildcard components/*) $(wildcard $(BUILD_CONFIG_DIR)/*)
	@echo "-------------------- Rebuilding Locally --------------------"
	@$(DELETE_SDKCONFIG_IF_OUTDATED)
	$(LOCAL_EXEC) && $(CMD)
clean:
	rm -rf ./build/ ./build_raft_artefacts/ $(BUILD_CONFIG_DIR)/sdkconfig || true
endif

ifeq ($(MONITOR),SerialMonitor.py)
flash: build
	$(PYTHON_FOR_FLASH_MONITOR) $(BUILD_DIR)/_deps/raftcore-src/scripts/flashUsingPartitionCSV.py $(BUILD_RAFT_ARTEFACTS_DIR)/partitions.csv $(BUILD_DIR) $(BUILD_CONFIG_NAME).bin $(PORT) -s $(BUILD_CONFIG_DIR)/sdkconfig -f spiffs.bin
	$(PYTHON_FOR_FLASH_MONITOR) $(BUILD_DIR)/_deps/raftcore-src/scripts/SerialMonitor.py $(PORT) -g
else ifeq ($(MONITOR),serial-monitor)
flash: build
	$(PYTHON_FOR_FLASH_MONITOR) $(BUILD_DIR)/_deps/raftcore-src/scripts/flashUsingPartitionCSV.py $(BUILD_RAFT_ARTEFACTS_DIR)/partitions.csv $(BUILD_DIR) $(BUILD_CONFIG_NAME).bin $(PORT) -s $(BUILD_CONFIG_DIR)/sdkconfig -f spiffs.bin
	serial-monitor -p $(PORT)
else
flash: build
	$(PYTHON_FOR_FLASH_MONITOR) $(BUILD_DIR)/_deps/raftcore-src/scripts/flashUsingPartitionCSV.py $(BUILD_RAFT_ARTEFACTS_DIR)/partitions.csv $(BUILD_DIR) $(BUILD_CONFIG_NAME).bin $(PORT) -s $(BUILD_CONFIG_DIR)/sdkconfig -f spiffs.bin
endif

.PHONY: build clean flash test

