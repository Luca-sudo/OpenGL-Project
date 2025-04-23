INCLUDES = include
LIBRARIES = -lglfw -lGL -lX11 -lpthread -lXrandr -lXi -ldl -lassimp -lz -lstdc++ -lm -limgui
FLAGS = -g

OUT_DIR = build

CC = g++

SRCS = $(wildcard src/*.c)

project: $(SRCS) main.cpp
	mkdir -p $(OUT_DIR)
	$(CC) $(FLAGS) -I$(INCLUDES) $^ -o $(OUT_DIR)/project $(LIBRARIES)


clean:
	rm -rf $(OUT_DIR)
