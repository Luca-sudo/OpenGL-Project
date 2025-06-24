INCLUDES = include include/imgui include/imgui/backends
LIBRARIES = -lglfw -lGL -lX11 -lpthread -lXrandr -lXi -ldl -lassimp -lz -lstdc++ -lm -limgui
FLAGS = -g -L.

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

IMGUI_OBJS = $(IMGUI_SRCS:%.cpp=obj/%.o)

ALL_SRCS = $(SRCS) main.cpp

all: libimgui.a project move_textures

project: $(ALL_SRCS)
	mkdir -p $(OUT_DIR)
	$(CC) $(FLAGS) $(addprefix -I, $(INCLUDES)) $^ -o $(OUT_DIR)/project $(LIBRARIES)

obj/%.o: %.cpp
	@mkdir -p $(@D)
	$(CC) -Iinclude/imgui -c $< -o $@

libimgui.a: $(IMGUI_OBJS)
	ar rcs libimgui.a $^

move_textures:
	cp -r textures $(OUT_DIR)/

clean:
	rm -rf $(OUT_DIR)
	rm -rf obj
	rm libimgui.a

.PHONY: move_textures
