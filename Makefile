LIBRARIES = -lglfw -lGL -lX11 -lpthread -lXrandr -lXi -ldl -lassimp -lz -lstdc++
FLAGS = -g

OUT_DIR = build

CC = gcc

SRCS = $(wildcard src/*.c)

project: $(SRCS) main.c
	mkdir -p $(OUT_DIR)
	$(CC) $(FLAGS) -I$(INCLUDES) $^ -o $(OUT_DIR)/project $(LIBRARIES)


clean:
	rm -rf $(OUT_DIR)
