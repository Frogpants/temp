#include <vector>
#include <iostream>
#include <cstdlib>
#include <ctime>

inline float random01() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

#define random random01
#include "neuron.hpp"
#undef random

std::vector<std::string> memory = {};

void setup() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    neurons.clear();

    constexpr int neuronCount = 24;
    neurons.reserve(neuronCount);

    for (int i = 0; i < neuronCount; i++) {
        Neuron neuron;
        neuron.id = i;
        neuron.active = i == 0 ? 1.0f : 0.0f;
        neuron.pos = vec3(randRange(-1.0f, 1.0f), randRange(-1.0f, 1.0f), randRange(-1.0f, 1.0f));
        neurons.push_back(neuron);
    }

    for (int i = 0; i < neuronCount; i++) {
        Neuron neuron = neurons[i];
        Neuron next = neurons[(i + 1) % neuronCount];

        neuron.connections.push_back(connect(neuron, next));

        if ((i % 4) == 0) {
            Neuron skip = neurons[(i + 4) % neuronCount];
            neuron.connections.push_back(connect(neuron, skip));
        }

        neurons[i] = neuron;
    }
}

void update(int frame) {
    std::vector<Signal> input = {
        { vec3(0.0f, 0.0f, 0.0f), 1.0f, frame % 30 == 0 ? 1.0f : 0.25f }
    };

    inject_data(input);
    propagate();
    adapt();

    if ((frame % 8) == 0) {
        grow();
    }

    if ((frame % 16) == 0) {
        prune();
    }
}

int main() {
    setup();

    constexpr int frameCount = 240;
    for (int frame = 0; frame < frameCount; frame++) {
        update(frame);

        if ((frame % 30) == 0 && !neurons.empty()) {
            std::cout << "frame " << frame << " neurons=" << neurons.size() << " active=" << neurons.front().active << '\n';
        }
    }

    return 0;
}
