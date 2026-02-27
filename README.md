# Procedural Terrain Generator 🌄

A small experimental application for generating and displaying procedural terrain using OpenGL, GLFW and ImGui.  
Parts of the renderer, GUI and noise algorithms are under active development – consider this a work‑in‑progress.

## Features

- OpenGL-based terrain rendering  
- ImGui interface for tweaking parameters  
- GLSL shaders for tessellation / displacement  
- GLFW window/input handling  
- Modular code in `renderer/`, `terrain/`, `shaders/` etc.

## Build & Run

1. **Dependencies:**  
   - C++ compiler with C++17 support  
   - `glfw3`, OpenGL headers (bundled under `include/`)  
   - Make (project uses `Makefile` on Windows/Unix)

2. **Compile:**
   ```sh
   make
   ```

3. **Launch:**
   ```sh
   ./ptg
   ```

   (On Windows the executable is `ptg.exe`.)

## Structure

```
├── ptg.cpp            # entry point
├── renderer/          # rendering utilities & camera
├── terrain/           # terrain generation logic
├── shaders/           # GLSL programs
├── include/           # third-party libs (glad, GLFW, glm)
├── backends/          # ImGui GLFW/OpenGL backends
```

## Status

⚠️ **WIP:** many systems are incomplete, APIs may change, and the code may be reorganized. Idk if some of this stuff even works.

---
