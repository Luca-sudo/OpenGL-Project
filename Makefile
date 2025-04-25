INCLUDES = include include/imgui include/imgui/backends
LIBRARIES = -lglfw -lGL -lX11 -lpthread -lXrandr -lXi -ldl -lassimp -lz -lstdc++ -lm
FLAGS = -g

OUT_DIR = build

CC = g++

SRCS = $(wildcard src/*.c)
IMGUI_SRCS = \
	include/imgui/imgui.cpp \
	include/imgui/imgui_draw.cpp \
	include/imgui/imgui_widgets.cpp \
	include/imgui/imgui_tables.cpp \
	include/imgui/backends/imgui_impl_glfw.cpp \
	include/imgui/backends/imgui_impl_opengl3.cpp

ALL_SRCS = $(SRCS) main.cpp $(IMGUI_SRCS)

project: $(ALL_SRCS)
	mkdir -p $(OUT_DIR)
	$(CC) $(FLAGS) $(addprefix -I, $(INCLUDES)) $^ -o $(OUT_DIR)/project $(LIBRARIES)

clean:
	rm -rf $(OUT_DIR)
