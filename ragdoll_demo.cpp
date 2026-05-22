#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <ctime>

inline float random01() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

#define random random01
#include "body.hpp"
#include "ragdoll.hpp"
#undef random

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    Body body;
    Ragdoll ragdoll;

    constexpr int frames = 240;
    for (int f = 0; f < frames; ++f) {
        // synthesize an oscillating BodyCommand as if neurons produced it
        BodyCommand cmd;
        for (int i = 0; i < outputSlotCount; ++i) {
            float t = static_cast<float>(f) * 0.02f;
            cmd.outputs[i] = std::sin(t * (0.5f + i * 0.17f));
        }

        body.apply(cmd);

        // step ragdoll using current body state
        ragdoll.step(0.016f, body.state);

        if ((f % 30) == 0) {
            std::cout << "frame=" << f << " ";
            ragdoll.debugPrint();
            std::cout << " body movement=" << body.state.movement.x << "," << body.state.movement.y << "," << body.state.movement.z << "\n";
        }

        // small sleep to simulate realtime (optional)
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    return 0;
}
