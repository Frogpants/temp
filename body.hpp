#pragma once

#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

#include "neuron.hpp"

// Body interpreter: does NOT hardcode neuron semantics.
// It learns a projection from anonymous neuron output slots into actuator channels.
// Actuator channels (movement.x/y/z, speech, expression) exist, but their mapping
// from neuron outputs is governed by a small, trainable weight matrix initialized
// randomly. The body also emits feedback signals (thought/emotion/proprioception)
// back into the sensory stream so the network can self-organize semantics.

struct BodyState {
    vec3 movement = vec3(0.0f);
    float speech = 0.0f;
    float expression = 0.0f;
    float thought = 0.0f;   // internal scalar reflecting "thought"
    float emotion = 0.0f;   // internal scalar reflecting "emotion"
};

struct Body {
    BodyState state;

    // Number of concrete actuators we expose: movement xyz + speech + expression
    static constexpr int actuatorCount = 5;

    // Weight matrix: actuatorCount x outputSlotCount
    std::vector<std::vector<float>> weights;

    // Last command outputs stored for learning updates
    BodyCommand lastCommand{};

    // Simple RNG for initialization
    std::mt19937 rng{12345};

    Body() {
        std::uniform_real_distribution<float> d(-0.05f, 0.05f);
        weights.resize(actuatorCount);
        for (int i = 0; i < actuatorCount; ++i) {
            weights[i].resize(outputSlotCount);
            for (int j = 0; j < outputSlotCount; ++j) {
                weights[i][j] = d(rng);
            }
        }
    }

    // Interpret anonymous outputs into actuators using the trainable projection.
    void apply(const BodyCommand& command) {
        lastCommand = command;

        std::vector<float> actuators(actuatorCount, 0.0f);
        for (int i = 0; i < actuatorCount; ++i) {
            float sum = 0.0f;
            for (int j = 0; j < outputSlotCount; ++j) {
                sum += weights[i][j] * command.outputs[j];
            }
            // simple squashing to keep values stable
            actuators[i] = std::tanh(sum);
        }

        // assign actuators to body state
        state.movement = vec3(actuators[0], actuators[1], actuators[2]);
        state.speech = actuators[3];
        state.expression = actuators[4];

        // compute internal thought/emotion as simple functions of actuators
        state.thought = 0.5f * (state.speech + (state.movement.x + state.movement.y + state.movement.z) / 3.0f);
        state.emotion = 0.5f * (state.expression + std::fabs(state.movement.y));
    }

    // Provide feedback signals to the neuron network derived from body state.
    // These are generic and parameterizable; they are how the body guides
    // the network without hardcoding neuron semantics.
    std::vector<Signal> feedbackSignals() const {
        std::vector<Signal> signals;

        // Thought feeds back into association areas as a low-intensity signal
        signals.push_back({ SenseType::Smell, BrainSection::Association, vec3(0.0f, 0.0f, 0.0f), state.thought, 0.05f });

        // Proprioceptive touch-like feedback from movement
        signals.push_back({ SenseType::Touch, BrainSection::Sensory, state.movement, std::fabs(state.movement.y), 0.06f });

        // Auditory feedback from speech
        signals.push_back({ SenseType::Hearing, BrainSection::Association, vec3(state.speech, 0.0f, 0.0f), state.speech, 0.04f });

        // Visuo-like feedback from expression
        signals.push_back({ SenseType::Vision, BrainSection::Output, vec3(state.expression, 0.0f, 0.0f), state.expression, 0.04f });

        // Emotion broadcast (low amplitude) into association
        signals.push_back({ SenseType::Taste, BrainSection::Association, vec3(0.0f, 0.0f, 0.0f), state.emotion, 0.03f });

        return signals;
    }

    // Simple learning rule: update projection weights based on a scalar reward.
    // The user or environment can call this with positive/negative reward so
    // the body adapts which outputs drive which actuators. This keeps semantics
    // emergent rather than hardcoded.
    void learn(float reward, float lr = 1e-3f) {
        for (int i = 0; i < actuatorCount; ++i) {
            float act = 0.0f;
            // recompute actuator pre-squash (approx) using lastCommand
            for (int j = 0; j < outputSlotCount; ++j) act += weights[i][j] * lastCommand.outputs[j];
            float post = std::tanh(act);
            for (int j = 0; j < outputSlotCount; ++j) {
                // Hebbian-like: correlate command component with actuator post-activation and scalar reward
                weights[i][j] += lr * reward * lastCommand.outputs[j] * post;
            }
        }
    }
};