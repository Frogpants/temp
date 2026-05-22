#include <vector>
#include <iostream>
#include <cstdlib>
#include <ctime>

inline float random01() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

#define random random01
#include "body.hpp"
#undef random

std::vector<std::string> memory = {};

Body body;

vec3 sense_position(SenseType sense) {
    switch (sense) {
        case SenseType::Vision: return vec3(-0.7f, 0.7f, 0.0f);
        case SenseType::Hearing: return vec3(0.7f, 0.7f, 0.0f);
        case SenseType::Smell: return vec3(-0.7f, -0.7f, 0.0f);
        case SenseType::Taste: return vec3(0.7f, -0.7f, 0.0f);
        case SenseType::Touch: return vec3(0.0f, 0.0f, 0.7f);
        default: return vec3(0.0f);
    }
}

std::vector<Signal> sensory_data(int frame) {
    std::vector<Signal> data = {};

    for (int sense = 0; sense < static_cast<int>(SenseType::Count); sense++) {
        Signal s;
        s.sense = static_cast<SenseType>(sense);
        s.pos = sense_position(s.sense);
        s.value = 0.35f + 0.15f * static_cast<float>((frame + sense) % 4);
        s.intensity = 0.5f + 0.1f * static_cast<float>((frame + sense * 3) % 3);

        data.push_back(s);
    }

    return data;
}

void setup() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    neurons.clear();

    constexpr int neuronCount = 250;
    neurons.reserve(neuronCount);

    for (int i = 0; i < neuronCount; i++) {
        Neuron neuron;
        neuron.id = i;
        neuron.pos = vec3(randRange(-1.0f, 1.0f), randRange(-1.0f, 1.0f), randRange(-1.0f, 1.0f));

        if (i < 80) {
            neuron.section = BrainSection::Sensory;
            neuron.active = i == 0 ? 1.0f : 0.0f;
        } else if (i < 190) {
            neuron.section = BrainSection::Association;
        } else {
            neuron.section = BrainSection::Output;
            neuron.active = 0.2f;
            for (int slot = 0; slot < outputSlotCount; slot++) {
                neuron.outputDrive[slot] = randRange(0.0f, 0.1f);
            }
        }

        neurons.push_back(neuron);
    }

    for (int i = 0; i < neuronCount; i++) {
        Neuron neuron = neurons[i];
        Neuron next = neurons[(i + 1) % neuronCount];

        neuron.connections.push_back(connect(neuron, next));

        if (neuron.section == BrainSection::Sensory) {
            neuron.connections.push_back(connect(neuron, neurons[80 + (i % 110)]));
        } else if (neuron.section == BrainSection::Association) {
            neuron.connections.push_back(connect(neuron, neurons[190 + (i % 60)]));
        } else if (neuron.section == BrainSection::Output) {
            neuron.connections.push_back(connect(neuron, neurons[i % 80]));
        }

        if ((i % 4) == 0) {
            Neuron skip = neurons[(i + 4) % neuronCount];
            neuron.connections.push_back(connect(neuron, skip));
        }

        neurons[i] = neuron;
    }
}

void update(int frame) {
    inject_data(sensory_data(frame));
    propagate();
    adapt();
    decay_sense_drive();
    grow();
    prune();

    BodyCommand command = collect_body_output();
    body.apply(command);
    inject_data(body.feedbackSignals());

    if ((frame % 30) == 0) {
        std::cout << "movement=" << body.state.movement.x << "," << body.state.movement.y << "," << body.state.movement.z
                  << " speech=" << body.state.speech
                  << " expression=" << body.state.expression
                  << " thought=" << body.state.thought
                  << " emotion=" << body.state.emotion
                  << '\n';

        std::cout << '\n';
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
