SRC_DIR = src
CC = g++-11
SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)
OBJ_NAME = main
INCLUDE_PATHS = -Iinclude
LIBRARY_PATHS = -Llib/SDL2 -Llib/SDL2_image
COMPILER_FLAGS = -std=c++2a -Wall -Wno-unknown-pragmas
LINKER_FLAGS = -lsdl2 -lsdl2_image

build:
	$(CC) $(COMPILER_FLAGS) $(LINKER_FLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) $(SRC_FILES) -o build/$(BUILD_NAME)/$(OBJ_NAME)

build-debug:
	$(MAKE) build BUILD_NAME=debug

build-release:
	$(MAKE) build BUILD_NAME=debug

debug:
	./build/debug/$(OBJ_NAME)

release:
	./build/release/$(OBJ_NAME)