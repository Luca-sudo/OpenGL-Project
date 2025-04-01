
INCLUDES = include
LIBRARIES = -lglfw -lGL -lX11 -lpthread -lXrandr -lXi -ldl

OUT_DIR = build

CC = gcc

project: src/glad.c main.c
	mkdir -p $(OUT_DIR)
	$(CC) -I$(INCLUDES) $^ -o $(OUT_DIR)/project $(LIBRARIES)
