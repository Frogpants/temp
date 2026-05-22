Ragdoll Viewer

This small viewer uses GLFW + OpenGL (legacy immediate mode) to visualize the `Ragdoll` skeleton from `ragdoll.hpp`.

Dependencies (on Debian/Ubuntu):

```bash
sudo apt-get install libglfw3-dev libglu1-mesa-dev libx11-dev
```

Build:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic ragdoll_viewer.cpp -o ragdoll_viewer -lglfw -lGL -lGLU -ldl -lpthread
```

Run:

```bash
./ragdoll_viewer
```

Controls:
- Left mouse drag: orbit camera
- Mouse wheel: zoom in/out
- W/A/S/D: move camera target
- Q/E: zoom out/in
- Space: pause simulation

Notes:
- If linking fails, install GLFW and GLU dev packages for your distro. On some systems you may need additional X11-related packages.
- This viewer draws lines for bones and points for joints; it's intended for debugging and visualization, not production rendering.
