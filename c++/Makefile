CC = g++-11
CFLAGS = -std=c++2a -Wall -Wno-unknown-pragmas -O3
LINKER_FLAGS = 
LIBRARY_PATHS = 

build:
	$(eval INCLUDE_PATHS = -I $(NAME))
	$(eval SRC_FILES = $(wildcard $(NAME)/src/*.cpp) $(wildcard $(NAME)/tests/*.cpp))

ifeq ($(NAME), rxd)
	$(eval INCLUDE_PATHS = -Irxd/include -Irxd/include/SDL2 -Irxd/include/SDL2_image)
	$(eval LIBRARY_PATHS = -Lrxd/lib/SDL2 -Lrxd/lib/SDL2_image)
	$(eval LINKER_FLAGS = -lsdl2 -lsdl2_image)
else ifeq ($(NAME), lpc)
	$(eval CFLAGS += -fconcepts-diagnostics-depth=2)
endif

ifeq ($(TEST), true)
	$(eval SRC_FILES := $(filter-out $(NAME)/main.cpp, $(SRC_FILES)))
else
	$(eval SRC_FILES := $(filter-out $(NAME)/tests/main.cpp, $(SRC_FILES)))
endif

	$(CC) $(CFLAGS) $(LINKER_FLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) $(SRC_FILES) -o $(NAME)/build/$(TYPE)/$(NAME)

run:
	./$(NAME)/build/$(TYPE)/$(NAME)