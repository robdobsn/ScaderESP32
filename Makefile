# Build Project
DOCKER ?= 1
WSL ?= $(if $(findstring Windows_NT,$(OS)),1,0)
ESP_IDF_PATH ?= ~/esp/esp-idf-v5.1.2
BUILD_BASE_FOLDER ?= build
BUILD_CONFIG_BASE_DIR = systypes
SYSTYPE ?= $(notdir $(firstword $(filter-out $(BUILD_CONFIG_BASE_DIR)/Common, $(wildcard $(BUILD_CONFIG_BASE_DIR)/*))))
BUILD_CONFIG_DIR = $(BUILD_CONFIG_BASE_DIR)/$(SYSTYPE)
COMMON_CONFIG_DIR = $(BUILD_CONFIG_BASE_DIR)/Common
BUILD_RAFT_ARTEFACTS_DIR ?= build_raft_artefacts
BUILD_DIR = $(BUILD_BASE_FOLDER)/$(SYSTYPE)
ROOTDIR = $(realpath $(CURDIR))
SDKCONFIG_DEFAULTS_FILE ?= $(BUILD_CONFIG_DIR)/sdkconfig.defaults
SDKCONFIG_FILE ?= $(BUILD_RAFT_ARTEFACTS_DIR)/sdkconfig
TARGET_BINARY = $(BUILD_DIR)/$(SYSTYPE).bin
# DOCKER_EXEC ?= docker run --rm -v $(ROOTDIR):/project -w /project espressif/idf:v5.1.2
DOCKER_EXEC = docker build -t raftbuilder . && docker run --rm -v $(ROOTDIR):/project -w /project raftbuilder
LOCAL_EXEC = . $(ESP_IDF_PATH)/export.sh
CMD ?= idf.py -B $(BUILD_DIR) build

ifeq ($(WSL),1)
	SERIAL_MONITOR ?= "raft.exe monitor"
	PYTHON_FOR_FLASH ?= python.exe
	PORT ?= COM3
else
	SERIAL_MONITOR ?= "./raft monitor"
	PYTHON_FOR_FLASH ?= python3
	PORT ?= /dev/ttyUSB0
endif

# Custom command to delete build folders
ifeq ($(DOCKER),1)
DELETE_BUILD_FOLDERS=\
	@echo "-------------------- Deleting build folders --------------------"; \
	$(DOCKER_EXEC) rm -rf ./build/ ./build_raft_artefacts/ || true
BUILD_TARGET=\
	@echo "-------------------- Building in Docker --------------------"; \
	$(DOCKER_EXEC) $(CMD)
else
DELETE_BUILD_FOLDERS=\
	@echo "-------------------- Deleting build folders --------------------"; \
	rm -rf ./build/ ./build_raft_artefacts/ || true
BUILD_TARGET=\
	@echo "-------------------- Building Locally --------------------"; \
	$(LOCAL_EXEC) && $(CMD)
endif

all: build

# Dependencies of target binary
$(TARGET_BINARY): $(wildcard $(BUILD_CONFIG_DIR)/*) $(wildcard $(COMMON_CONFIG_DIR)/*) $(wildcard $(COMMON_CONFIG_DIR)/FSImage/*) $(wildcard $(COMMON_CONFIG_DIR)/WebUI*)
	@$(DELETE_BUILD_FOLDERS)

clean:
	@$(DELETE_BUILD_FOLDERS)

build: $(TARGET_BINARY)
	@$(BUILD_TARGET)

ifneq ($(SERIAL_MONITOR),)
flash: build
	@$(PYTHON_FOR_FLASH) $(BUILD_DIR)/_deps/raftcore-src/scripts/flashUsingPartitionCSV.py $(BUILD_RAFT_ARTEFACTS_DIR)/partitions.csv $(BUILD_DIR) $(SYSTYPE).bin $(PORT) -s $(SDKCONFIG_FILE) -f fs.bin
	$(SERIAL_MONITOR) $(PORT) -l
else
flash: build
	@$(PYTHON_FOR_FLASH) $(BUILD_DIR)/_deps/raftcore-src/scripts/flashUsingPartitionCSV.py $(BUILD_RAFT_ARTEFACTS_DIR)/partitions.csv $(BUILD_DIR) $(SYSTYPE).bin $(PORT) -s $(SDKCONFIG_FILE) -f fs.bin
endif

.PHONY: build clean flash test
