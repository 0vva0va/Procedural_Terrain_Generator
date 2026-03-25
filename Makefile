# --- Project Configuration ---
PROJECT_NAME := PTGen
EXECUTABLE := $(PROJECT_NAME)

# Detect OS
UNAME_S := $(shell uname -s)
ifeq ($(OS),Windows_NT)
    EXECUTABLE := $(PROJECT_NAME).exe
    DETECTED_OS := Windows
else
    DETECTED_OS := $(UNAME_S)
endif

# --- Directories ---
SRC_DIR := .
EXT_DIR := External
EXT_EXT_DIR := $(EXT_DIR)/External
INC_DIR := $(EXT_DIR)/Include
SHADER_DIR := Shaders
SCRIPTS_DIR := Scripts
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
DEP_DIR := $(BUILD_DIR)/dep

# --- vpath Directive ---
vpath %.cpp $(SRC_DIR) $(INC_DIR) $(SCRIPTS_DIR) $(EXT_EXT_DIR)/glad $(EXT_EXT_DIR)/imgui
vpath %.c $(EXT_EXT_DIR)/glad

# --- Source Files ---
MAIN_SOURCES := $(SRC_DIR)/PTGen.cpp

# Camera implementation
CAMERA_SOURCES := $(INC_DIR)/Camera.cpp

# Script sources
SCRIPT_SOURCES := $(wildcard $(SCRIPTS_DIR)/*/*.cpp)

# GLAD loader
GLAD_SOURCES := $(EXT_EXT_DIR)/glad/glad.c

# ImGui sources
IMGUI_SOURCES := \
    $(EXT_EXT_DIR)/imgui/imgui.cpp \
    $(EXT_EXT_DIR)/imgui/imgui_demo.cpp \
    $(EXT_EXT_DIR)/imgui/imgui_draw.cpp \
    $(EXT_EXT_DIR)/imgui/imgui_widgets.cpp \
    $(EXT_EXT_DIR)/imgui/imgui_tables.cpp \
    $(EXT_EXT_DIR)/imgui/imgui_impl_glfw.cpp \
    $(EXT_EXT_DIR)/imgui/imgui_impl_opengl3.cpp

# Combine all sources
ALL_SOURCES := $(MAIN_SOURCES) $(CAMERA_SOURCES) $(SCRIPT_SOURCES) $(GLAD_SOURCES) $(IMGUI_SOURCES)

# Convert source files to object files
OBJECTS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(ALL_SOURCES))
OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(OBJECTS))

# Dependency files (one .d per .o, mirroring the obj tree under dep/)
DEPS := $(patsubst $(OBJ_DIR)/%.o,$(DEP_DIR)/%.d,$(OBJECTS))

# --- Include Paths ---
INCLUDE_FLAGS := \
    -I$(EXT_EXT_DIR) \
    -I$(INC_DIR) \
    -I$(SCRIPTS_DIR) \
    -I$(SHADER_DIR)/Loaders \
    -I$(EXT_EXT_DIR)/glm \
    -I$(EXT_EXT_DIR)/imgui \
    -I$(EXT_EXT_DIR)/GLFW \
    -I$(EXT_EXT_DIR)/glad \
    -I$(EXT_EXT_DIR)/KHR

# --- Compiler Configuration ---
CXX := g++
CC := gcc

# -MMD  : write a .d file listing header dependencies (skips system headers)
# -MP   : add a phony target for each header so a deleted header doesn't break Make
DEP_FLAGS = -MMD -MP -MF $(DEP_DIR)/$*.d

CXXFLAGS := -std=c++20 -Wall -Wextra -O2 $(INCLUDE_FLAGS)
CFLAGS   := -std=c99  -Wall         -O2 $(INCLUDE_FLAGS)

# Platform-specific settings
ifeq ($(DETECTED_OS),Windows)
    LDFLAGS := -lglfw3 -lopengl32 -lgdi32 -luser32 -lkernel32
else ifeq ($(DETECTED_OS),Darwin)
    LDFLAGS   := -lglfw -framework OpenGL
    CXXFLAGS  += -fPIC
    CFLAGS    += -fPIC
else
    LDFLAGS   := -lglfw -lGL
    CXXFLAGS  += -fPIC
    CFLAGS    += -fPIC
endif

# --- Targets ---
.PHONY: all clean rebuild run help

all: $(BUILD_DIR) $(OBJ_DIR) $(DEP_DIR) $(EXECUTABLE)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(DEP_DIR):
	@mkdir -p $(DEP_DIR)

# Link executable
$(EXECUTABLE): $(OBJECTS)
	@echo "Linking $@..."
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "✓ Build complete: $@"

# Compile C++ sources — also writes dep file via DEP_FLAGS
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@) $(dir $(DEP_DIR)/$*.d)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(DEP_FLAGS) -c $< -o $@

# Compile C sources — also writes dep file via DEP_FLAGS
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@) $(dir $(DEP_DIR)/$*.d)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(DEP_FLAGS) -c $< -o $@

# Pull in the auto-generated dependency rules.
# The leading '-' suppresses errors on first build (no .d files yet).
-include $(DEPS)

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@rm -f $(EXECUTABLE)
	@echo "✓ Clean complete"

# Clean and rebuild
rebuild: clean all

# Run the executable
run: $(EXECUTABLE)
	@echo "Running $(EXECUTABLE)..."
	./$(EXECUTABLE)

# Default target
.DEFAULT_GOAL := all
