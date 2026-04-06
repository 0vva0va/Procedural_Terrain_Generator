# Procedural Terrain Generator 🌄

A small experimental application for generating and rendering procedural terrain with OpenGL, GLFW and ImGui.
This project explores noise-based terrain synthesis, dynamic lighting, and real-time effect passes.  
⚠️ **WIP:** ongoing work, API and architecture are subject to change.

## Features

- OpenGL-based real-time procedural terrain mesh rendering
- ImGui parameter panel for noise, shading, fog, shadows, and camera
- Depth+shadow pass, normal mapping, and full-screen compositing in GLSL
- Fractal Brownian Motion (FBM) noise stack for multi-octave terrain detail
- Modular script-based architecture in `Scripts/` and `Shaders/`
- Included external libraries: GLAD, GLFW, GLM, ImGui, stb_image

## Project layout

- `PTGen.cpp` : main entry point
- `Include/Camera.*` : camera controls and view matrices
- `Scripts/` : core features by subsystem
  - `CascadeShadows/` : shadow map generation and cascade logic
  - `Skybox/` : skybox rendering
  - `Terrain/` : terrain mesh generation and rendering
- `Shaders/` : GLSL source files and compute shaders
- `External/External/` : vendor libraries (glad, imgui, GLFW headers, glm)
- `build/` : generated object files and dependency graphs

## Prerequisites

- C++ toolchain with C++17/20 support
- `make` utility (MinGW/WSL on Windows or native on Linux/macOS)
- Graphics drivers supporting OpenGL 3.3+

## Configuration

- Shader source directory: `Shaders/`
- Output executable: `PTGen` / `PTGen.exe`

## Status

⚠️ **WIP:** core systems are functional but still evolving. Some behavior may be incomplete (fog, FBM quality, performance tuning).

## References

- Fog implementation: [Inigo Quilez article on better fog](https://iquilezles.org/articles/fog/)
- FBM noise implementation: [Inigo Quilez article on noise derivatives](https://iquilezles.org/articles/morenoise/)

---
