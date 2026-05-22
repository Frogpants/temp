#pragma once

#include <array>
#include <vector>
#include <cmath>

#include "essentials.hpp"

inline float randRange(float a, float b) {
    return a + random() * (b - a);
}

enum class SenseType {
    Vision = 0,
    Hearing = 1,
    Smell = 2,
    Taste = 3,
    Touch = 4,
    Count = 5,
};

enum class BrainSection {
    Sensory = 0,
    Association = 1,
    Output = 2,
    Count = 3,
};

constexpr int outputSlotCount = 8;

inline float sanitizeFloat(float value, float fallback = 0.0f) {
    return std::isfinite(value) ? value : fallback;
}

// Global learning signals shared by the simulation loop.
// learningSignal modulates Hebbian adaptation strength, and outputNoiseScale
// adds small exploration jitter to the generated control outputs.
inline float learningSignal = 1.0f;
inline float outputNoiseScale = 0.0f;

inline int senseIndex(SenseType sense) {
    return static_cast<int>(sense);
}

inline const char* senseName(SenseType sense) {
    switch (sense) {
        case SenseType::Vision: return "vision";
        case SenseType::Hearing: return "hearing";
        case SenseType::Smell: return "smell";
        case SenseType::Taste: return "taste";
        case SenseType::Touch: return "touch";
        default: return "unknown";
    }
}

inline const char* sectionName(BrainSection section) {
    switch (section) {
        case BrainSection::Sensory: return "sensory";
        case BrainSection::Association: return "association";
        case BrainSection::Output: return "output";
        default: return "unknown";
    }
}

template <size_t N>
inline int dominantIndex(const std::array<float, N>& values, float& bestValue) {
    int bestIndex = -1;
    bestValue = 0.0f;

    for (int index = 0; index < static_cast<int>(N); index++) {
        if (values[index] > bestValue) {
            bestValue = values[index];
            bestIndex = index;
        }
    }

    return bestIndex;
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
    BrainSection section = BrainSection::Association;
    std::vector<Connection> connections = {};
    std::vector<float> history = {};
    std::array<float, static_cast<int>(SenseType::Count)> senseDrive = {};
    std::array<float, static_cast<int>(BrainSection::Count)> sectionDrive = {};
    std::array<float, outputSlotCount> outputDrive = {};
    float plastic = randRange(0.001, 0.05);
    float importance = 0.0;
    vec3 pos = vec3(randRange(-1.0, 1.0));
};

inline std::vector<Neuron> neurons = {};

struct Signal {
    SenseType sense = SenseType::Touch;
    BrainSection section = BrainSection::Sensory;
    vec3 pos;
    float value;
    float intensity;
};

struct BodyCommand {
    std::array<float, outputSlotCount> outputs = {};
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
        signal.value = sanitizeFloat(signal.value);
        signal.intensity = sanitizeFloat(signal.intensity);
        std::vector<Neuron> near = nearby(signal.pos, 0.02);
        int index = senseIndex(signal.sense);
        int sectionIndex = static_cast<int>(signal.section);

        for (int j = 0; j < near.size(); j++) {
            Neuron n = near[j];

            n.active += signal.value * signal.intensity;
            n.senseDrive[index] += signal.value * signal.intensity;
            n.sectionDrive[sectionIndex] += signal.value * signal.intensity;
            n.active = sanitizeFloat(n.active);
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
                neurons[c.target].active = sanitizeFloat(neurons[c.target].active);
            }
            c.usage += abs(trans);
            c.usage = sanitizeFloat(c.usage);

            n.connections[j] = c;
        }

        n.active *= n.decay;
        n.active = std::clamp(sanitizeFloat(n.active), -5.0f, 5.0f);

        neurons[i] = n; 
    }
}

inline void adapt() {
    for (int i = 0; i < neurons.size(); i++) {
        Neuron n = neurons[i];
        float modulator = std::clamp(sanitizeFloat(learningSignal, 1.0f), -1.0f, 2.0f);

        for (int j = 0; j < n.connections.size(); j++) {
            Connection c = n.connections[j];
            if (c.target < 0 || c.target >= neurons.size()) {
                continue;
            }

            Neuron target = neurons[c.target];

            float corr = n.active * target.active;

            c.weight += corr * c.plastic * modulator;
            c.weight = std::clamp(sanitizeFloat(c.weight), -2.0f, 2.0f);
            n.connections[j] = c;
        }

        neurons[i] = n; 
    }
}

inline void grow() {
    for (int i = 0; i < neurons.size(); i++) {
        Neuron n = neurons[i];
        float strongestDrive = 0.0f;
        int strongestSense = -1;
        int strongestSection = -1;

        strongestSense = dominantIndex(n.senseDrive, strongestDrive);

        float sectionDrive = 0.0f;
        strongestSection = dominantIndex(n.sectionDrive, sectionDrive);
        if (sectionDrive > strongestDrive) {
            strongestDrive = sectionDrive;
        }

        if (n.active > 0.5 && strongestDrive > 0.2f) {
            Neuron neur;
            neur.id = neurons.size();
            neur.pos = n.pos + vec3(randRange(-0.05f, 0.05f), randRange(-0.05f, 0.05f), randRange(-0.05f, 0.05f));

            if (strongestSection == static_cast<int>(BrainSection::Output)) {
                neur.section = BrainSection::Output;
                for (int slot = 0; slot < outputSlotCount; slot++) {
                    neur.outputDrive[slot] = randRange(0.0f, 0.1f);
                }
            } else if (strongestSection == static_cast<int>(BrainSection::Sensory) || strongestSense >= 0) {
                neur.section = BrainSection::Sensory;
            } else {
                neur.section = BrainSection::Association;
            }

            if (strongestSense >= 0) {
                neur.senseDrive[strongestSense] = strongestDrive * 0.5f;
            }

            if (strongestSection >= 0) {
                neur.sectionDrive[strongestSection] = strongestDrive * 0.5f;
            }

            n.connections.push_back(connect(n, neur));
            std::vector<Neuron> near = nearby(neur.pos, 0.02);

            for (int j = 0; j < near.size(); j++) {
                Neuron candidate = near[j];
                int candidateSense = -1;
                float candidateDrive = 0.0f;

                candidateSense = dominantIndex(candidate.senseDrive, candidateDrive);

                float candidateSectionDrive = 0.0f;
                int candidateSection = dominantIndex(candidate.sectionDrive, candidateSectionDrive);
                if (candidateSectionDrive > candidateDrive) {
                    candidateDrive = candidateSectionDrive;
                }

                float connectionChance = 0.1f;

                if (candidate.section == neur.section) {
                    connectionChance = 0.4f;
                } else if (candidateSection == strongestSection) {
                    connectionChance = 0.3f;
                } else if (candidateSense == strongestSense) {
                    connectionChance = 0.25f;
                }

                if (randRange(0.0f, 1.0f) < connectionChance) {
                    neur.connections.push_back(connect(neur, candidate));
                }
            }

            if (strongestSense >= 0) {
                n.senseDrive[strongestSense] *= 0.5f;
                for (int sense = 0; sense < static_cast<int>(SenseType::Count); sense++) {
                    if (sense != strongestSense) {
                        n.senseDrive[sense] *= 0.9f;
                    }
                }
            }

            if (strongestSection >= 0) {
                n.sectionDrive[strongestSection] *= 0.5f;
                for (int section = 0; section < static_cast<int>(BrainSection::Count); section++) {
                    if (section != strongestSection) {
                        n.sectionDrive[section] *= 0.9f;
                    }
                }
            }

            neurons.push_back(neur);
        }
    }
}

inline void decay_sense_drive() {
    for (int i = 0; i < neurons.size(); i++) {
        Neuron n = neurons[i];
        n.active = sanitizeFloat(n.active);

        for (int sense = 0; sense < static_cast<int>(SenseType::Count); sense++) {
            n.senseDrive[sense] *= 0.98f;
            n.senseDrive[sense] = sanitizeFloat(n.senseDrive[sense]);
        }

        for (int section = 0; section < static_cast<int>(BrainSection::Count); section++) {
            n.sectionDrive[section] *= 0.98f;
            n.sectionDrive[section] = sanitizeFloat(n.sectionDrive[section]);
        }

        for (int slot = 0; slot < outputSlotCount; slot++) {
            n.outputDrive[slot] *= 0.98f;
            n.outputDrive[slot] = sanitizeFloat(n.outputDrive[slot]);
        }

        neurons[i] = n;
    }
}

inline BodyCommand collect_body_output() {
    BodyCommand command;
    std::array<float, outputSlotCount> counts = {};

    for (int i = 0; i < neurons.size(); i++) {
        const Neuron& neuron = neurons[i];

        if (neuron.section == BrainSection::Output) {
            for (int slot = 0; slot < outputSlotCount; slot++) {
                float contribution = neuron.active * neuron.outputDrive[slot];
                if (std::isfinite(contribution)) {
                    command.outputs[slot] += contribution;
                    counts[slot] += 1.0f;
                }
            }
        }
    }

    if (outputNoiseScale > 0.0f) {
        for (float &value : command.outputs) {
            value += randRange(-outputNoiseScale, outputNoiseScale);
        }
    }

    for (float &value : command.outputs) {
        float count = counts[&value - &command.outputs[0]];
        if (count > 0.0f) {
            value /= count;
        }
        value = std::clamp(sanitizeFloat(value), -0.35f, 0.35f);
    }

    return command;
}

inline void mutate_generation(float scale) {
    if (scale <= 0.0f) return;

    for (Neuron &neuron : neurons) {
        neuron.threshold = std::clamp(sanitizeFloat(neuron.threshold + randRange(-scale, scale)), 0.05f, 2.0f);
        neuron.decay = std::clamp(sanitizeFloat(neuron.decay + randRange(-scale * 0.2f, scale * 0.2f)), 0.8f, 0.9999f);

        if (neuron.section == BrainSection::Output) {
            for (int slot = 0; slot < outputSlotCount; ++slot) {
                neuron.outputDrive[slot] = std::clamp(
                    sanitizeFloat(neuron.outputDrive[slot] + randRange(-scale * 2.5f, scale * 2.5f)),
                    -0.35f,
                    0.35f
                );

                if (random() < 0.08f) {
                    neuron.outputDrive[slot] = randRange(-0.3f, 0.3f);
                }
            }
        }

        for (Connection &connection : neuron.connections) {
            connection.weight = std::clamp(
                sanitizeFloat(connection.weight + randRange(-scale * 1.5f, scale * 1.5f)),
                -2.0f,
                2.0f
            );
        }
    }
}

void prune() {
    for (int i = 0; i < neurons.size(); i++) {
        Neuron n = neurons[i];

        std::vector<Connection> result = {};
        for (int j = 0; j < n.connections.size(); j++) {
            Connection c = n.connections[j];
            if (abs(c.weight) > 0.01) {
                result.push_back(c);
            }
        }
        n.connections = result;

        neurons[i] = n;
    }
}

