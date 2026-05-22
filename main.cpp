#include "window.hpp"
#include <iostream>


int main() {
    Window w;
    if (!w.create(1280, 720, "Test")) {
        std::cerr << "failed to create window\n";
        return 1;
    }

    const std::array<window::Vertex, 3> triangle = {
        window::Vertex{{-0.6f, -0.6f, 0.0f}, {1.0f, 0.2f, 0.2f, 1.0f}, {0.0f, 0.0f}},
        window::Vertex{{ 0.6f, -0.6f, 0.0f}, {0.2f, 1.0f, 0.2f, 1.0f}, {1.0f, 0.0f}},
        window::Vertex{{ 0.0f,  0.6f, 0.0f}, {0.2f, 0.2f, 1.0f, 1.0f}, {0.5f, 1.0f}},
    };

    while (!w.shouldClose()) {
        w.pumpEvents();
        w.clear(0.08f, 0.08f, 0.12f, 1.0f);
        w.drawTriangles(triangle);
        w.present();
    }

    return 0;
}