SRC_DIR = src
CC = g++
SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)
OBJ_NAME = main
INCLUDE_PATHS = -Iinclude
LIBRARY_PATHS = -Llib
COMPILER_FLAGS = -std=c++2a -Wall
LINKER_FLAGS = -lsdl2

build:
	$(CC) $(COMPILER_FLAGS) $(LINKER_FLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) $(SRC_FILES) -o $(BUILD_DIR)/$(OBJ_NAME)

debug:
	$(MAKE) build BUILD_DIR=build/debug
	./build/debug/$(OBJ_NAME)

release:
	$(MAKE) build BUILD_DIR=build/release
	./build/release/$(OBJ_NAME)