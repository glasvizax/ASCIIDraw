# ASCIIDraw: 3D Software Rasterizer
![alt text](https://img.shields.io/badge/C++-20-blue.svg)

![alt text](https://img.shields.io/badge/CMake-3.30-green.svg)

![alt text](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)

ASCIIDraw is a CPU-only, from-scratch 3D graphics pipeline that renders fully textured and shaded 3D models directly into the terminal using ASCII characters.
This project was built entirely without graphics APIs (like OpenGL, Vulkan, or DirectX) to gain a deep, under-the-hood understanding of how GPUs process geometry, rasterize triangles, and interpolate data.
# Showcase
![Osaka rendering showcase](osaka_showcase.gif)

# Build Instructions
### Prerequisites:
1. Assimp 
2. CMake 3.30 or higher 
3. C++20 compatible compiler (MSVC, GCC, Clang)
### Building:
```Bash
git clone https://github.com/glasvizax/ASCIIDraw.git
cd ASCIIDraw

cmake -B build -S .

cmake --build build --config Release
```
## Download & Play
Don't want to build from source? Download the ready-to-use Windows installer:
👉 **[Download ASCIIDraw NSIS Installer (v1.0.0)](https://github.com/glasvizax/ASCIIDraw/releases/tag/v1.0)**

*Note: Since the installer is not digitally signed, Windows Defender SmartScreen might show a warning. Click "More info" -> "Run anyway".*

# Controls 
* **W / S** - Move Forward / Backward
* **A / D** - Move Left / Right
* **Q / E** - Turn Camera Left / Right
* **Z / X** - Look Down / Up
* **O** - Exit

# Bonus: Multithreaded Particle Engine
Initially developed as a technical test for a AAA game studio, the repository also includes a highly optimized, CPU-based particle system (`ParticlesEngine.cpp`).

**Engineering Highlights:**
* **Double Buffering:** Front and back buffers for particle state to separate update/render phases.
* **Lock-Free Allocations:** Uses `std::atomic::fetch_add` for concurrent particle array writes, eliminating mutex bottlenecks.
* **Thread-Local RNG:** Implements `thread_local std::mt19937` to prevent thread contention during burst generation.
# Acknowledgments
I was inspired by many projects online, like so: \
[Scratchapixel](https://www.scratchapixel.com/index.html)\
[tinyrenderer](https://haqr.eu/tinyrenderer/) \
[LearnOpenGL](https://learnopengl.com/Introduction) \
[Game Engine. Life EXE](https://www.youtube.com/playlist?list=PL2XQZYeh2Hh-pHTCE0OVpFlnh2nawMwGs) \
[3Blue1Brown](https://www.youtube.com/@3blue1brown)
