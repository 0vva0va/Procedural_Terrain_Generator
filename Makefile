# Detect OS
ifeq ($(OS),Windows_NT)
    # Windows
    EXEC = terrain_generator.exe
    RUN_CMD = .\$(EXEC)
    RM = del /Q
else
    # Linux / macOS
    EXEC = terrain_generator
    RUN_CMD = ./$(EXEC)
    RM = rm -f
endif

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -O3 -Wall -Iinclude -Iimgui -Ibackends -Irenderer -Iterrain -Ishaders -Iconfig
LDFLAGS = -Llib -lglfw3 -lopengl32 -lgdi32

# Source files
SRC_CPP = ptg.cpp \
          renderer/camera.cpp \
          terrain/terrain.cpp \
          imgui/imgui.cpp \
          imgui/imgui_demo.cpp \
          imgui/imgui_draw.cpp \
          imgui/imgui_tables.cpp \
          imgui/imgui_widgets.cpp \
          backends/imgui_impl_glfw.cpp \
          backends/imgui_impl_opengl3.cpp \
          config/imgui_config.cpp

SRC_C = include/glad/glad.c

# Object files
OBJ_CPP = $(SRC_CPP:.cpp=.o)
OBJ_C = $(SRC_C:.c=.o)
OBJ = $(OBJ_CPP) $(OBJ_C)

# Default target
all: $(EXEC)

# Build executable
$(EXEC): $(OBJ)
	$(CXX) -o $(EXEC) $(OBJ) $(LDFLAGS)

# Compile C++ object files
ptg.o: ptg.cpp renderer/camera.h terrain/terrain.h shaders/shader_t.h config/imgui_config.h
renderer/camera.o: renderer/camera.cpp renderer/camera.h
terrain/terrain.o: terrain/terrain.cpp terrain/terrain.h
shaders/shader.o: shaders/shader_t.h
imgui/imgui.o: imgui/imgui.cpp imgui/imgui.h
imgui/imgui_demo.o: imgui/imgui_demo.cpp imgui/imgui.h
imgui/imgui_draw.o: imgui/imgui_draw.cpp imgui/imgui.h
imgui/imgui_tables.o: imgui/imgui_tables.cpp imgui/imgui.h
imgui/imgui_widgets.o: imgui/imgui_widgets.cpp imgui/imgui.h
backends/imgui_impl_glfw.o: backends/imgui_impl_glfw.cpp imgui/imgui.h backends/imgui_impl_glfw.h
backends/imgui_impl_opengl3.o: backends/imgui_impl_opengl3.cpp imgui/imgui.h backends/imgui_impl_opengl3.h
config/imgui_config.o: config/imgui_config.cpp config/imgui_config.h imgui/imgui.h

# Compile C object files
include/glad/glad.o: include/glad/glad.c include/glad/glad.h

# Run the program
run: $(EXEC)
	$(RUN_CMD)

# Clean up
clean:
	$(RM) $(OBJ) $(EXEC)
