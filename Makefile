# Makefile for the Distributed Shared Memory project

# Variables
CXX = mpicxx
CXXFLAGS = -std=c++20 -g -Wall
SRC_DIR = src
BUILD_DIR = build
APP_NAME = main
CONFIG_FILE = config/dsm_config.json

# Default number of processes for mpirun.
# This will be read from the config file in a later step.
NP = 4

.PHONY: all build run clean

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@cmake -S . -B $(BUILD_DIR)
	@$(MAKE) -C $(BUILD_DIR)

run: build
	@echo "Running with $(NP) processes..."
	@mpirun -np $(NP) --oversubscribe $(BUILD_DIR)/$(APP_NAME) $(CONFIG_FILE)

clean:
	@rm -rf $(BUILD_DIR)

