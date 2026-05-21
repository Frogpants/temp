#pragma once

#include <vector>
#include <cmath>

#include "essentials.hpp"

inline float randRange(float a, float b) {
    return a + random() * (b - a);
}

struct Connection {
    int target = -1;
    float weight = randRange(-0.1, 0.1);
    float plastic = randRange(0.001, 0.02);
    float usage = 0.0;
};

struct Neuron {
    int id;
    float active = 0.0;
    float threshold = randRange(0.1, 1.0);
    float decay = randRange(0.9, 0.999);
    std::vector<Connection> connections = {};
    std::vector<float> history = {};
    float plastic = randRange(0.001, 0.05);
    float importance = 0.0;
    vec3 pos = vec3(randRange(-1.0, 1.0));
};

inline std::vector<Neuron> neurons = {};

struct Signal {
    vec3 pos;
    float value;
    float intensity;
};



inline Connection connect(const Neuron&, const Neuron& b) {
    Connection c;
    c.target = b.id;
    return c;
}

inline std::vector<Neuron> nearby(vec3 pos, float r) {
    std::vector<Neuron> result = {};
    for (int i = 0; i < neurons.size(); i++) {
        Neuron n = neurons[i];
        float dist = length(n.pos - pos);
        if (dist <= r && dist != 0.0) {
            result.push_back(n);
        }
    }

    return result;
}

inline void inject_data(std::vector<Signal> data) {
    for (int i = 0; i < data.size(); i++) {
        Signal signal = data[i];
        std::vector<Neuron> near = nearby(signal.pos, 0.02);

        for (int j = 0; j < near.size(); j++) {
            Neuron n = near[j];

            n.active += signal.value * signal.intensity;
            neurons[n.id] = n;
        }
    }
}

inline void propagate() {
    for (int i = 0; i < neurons.size(); i++) {
        Neuron n = neurons[i];
        float out = n.active * 0.5;

        for (int j = 0; j < n.connections.size(); j++) {
            Connection c = n.connections[j];

            float trans = out * c.weight;
            if (c.target >= 0 && c.target < neurons.size()) {
                neurons[c.target].active += trans;
            }
            c.usage += abs(trans);

            n.connections[j] = c;
        }

        n.active *= n.decay;

        neurons[i] = n; 
    }
}

inline void adapt() {
    for (int i = 0; i < neurons.size(); i++) {
        Neuron n = neurons[i];

        for (int j = 0; j < n.connections.size(); j++) {
            Connection c = n.connections[j];
            if (c.target < 0 || c.target >= neurons.size()) {
                continue;
            }

            Neuron target = neurons[c.target];

            float corr = n.active * target.active;

            c.weight += corr * c.plastic;
            n.connections[j] = c;
        }

        neurons[i] = n; 
    }
}

inline void grow() {
    for (int i = 0; i < neurons.size(); i++) {
        Neuron n = neurons[i];

        if (n.active > 0.5) {
            Neuron neur;
            neur.id = neurons.size();

            n.connections.push_back(connect(n, neur));
            std::vector<Neuron> near = nearby(neur.pos, 0.02);

            for (int j = 0; j < near.size(); j++) {
                if (random() < 0.2) {
                    neur.connections.push_back(connect(neur, near[j]));
                }
            }

            neurons.push_back(neur);
        }
    }
}

void prune() {
    for (int i = 0; i < neurons.size(); i++) {
        Neuron n = neurons[i];

        std::vector<Connection> result = {};
        for (int j = 0; j < n.connections.size(); j++) {
            Connection c = n.connections[j];
            if (abs(c.weight) > 0.001) {
                result.push_back(c);
            }
        }
        n.connections = result;

        neurons[i] = n;
    }
}

