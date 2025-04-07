
INCLUDES = include
LIBRARIES = -lglfw -lGL -lX11 -lpthread -lXrandr -lXi -ldl -lm

OUT_DIR = build

CC = gcc

project: src/glad.c main.c src/glb_parser.c src/cgltf.c
	mkdir -p $(OUT_DIR)
	$(CC) -I$(INCLUDES) $^ -o $(OUT_DIR)/project $(LIBRARIES)
