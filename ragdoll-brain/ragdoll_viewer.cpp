#include <vector>
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdlib>
#include <ctime>

#include <GLFW/glfw3.h>

#include "body.hpp"
#include "ragdoll.hpp"
#include "renderer.hpp"

static Camera camera;
static bool mouseDown = false;
static double lastX = 0, lastY = 0;

inline float random01() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

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

std::vector<Signal> sensory_data(int frame, const Ragdoll& ragdoll, const Body& body) {
    std::vector<Signal> data;

    for (int sense = 0; sense < static_cast<int>(SenseType::Count); ++sense) {
        Signal s;
        s.sense = static_cast<SenseType>(sense);
        s.pos = sense_position(s.sense);
        s.value = 0.25f + 0.1f * static_cast<float>((frame + sense) % 5);
        s.intensity = 0.4f + 0.08f * static_cast<float>((frame + sense * 3) % 3);
        data.push_back(s);
    }

    // Physical balance cues. These are the signals that help the controller
    // learn to stand instead of drifting into a random motor policy.
    const Bone& pelvis = ragdoll.bones[0];
    const Bone& torso = ragdoll.bones[1];
    const Bone& leftFoot = ragdoll.bones[8];
    const Bone& rightFoot = ragdoll.bones[10];

    float height = std::max(0.0f, pelvis.pos.y);
    float torsoUpright = std::max(-1.0f, std::min(1.0f, torso.pos.y - pelvis.pos.y));
    float velocityPenalty = std::min(1.0f, length(pelvis.vel) * 0.3f);
    float footContact = 1.0f - std::min(1.0f, std::fabs(leftFoot.pos.y) + std::fabs(rightFoot.pos.y));

    Signal balance;
    balance.sense = SenseType::Touch;
    balance.section = BrainSection::Association;
    balance.pos = pelvis.pos;
    balance.value = 0.5f * height + 0.5f * torsoUpright - velocityPenalty + 0.1f * footContact;
    balance.intensity = 0.25f;
    data.push_back(balance);

    Signal rewardCue;
    rewardCue.sense = SenseType::Taste;
    rewardCue.section = BrainSection::Association;
    rewardCue.pos = body.state.movement;
    rewardCue.value = body.state.emotion;
    rewardCue.intensity = 0.15f;
    data.push_back(rewardCue);

    // Collision touch inputs: inject contact-specific touch at the actual
    // collision point so the network can learn local body sensation.
    for (const ContactEvent& contact : ragdoll.contacts) {
        Signal touch;
        touch.sense = SenseType::Touch;
        touch.section = BrainSection::Sensory;
        touch.pos = contact.position;
        touch.value = contact.strength;
        touch.intensity = 0.3f;
        data.push_back(touch);
    }

    return data;
}

void setupBrain() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    neurons.clear();

    constexpr int neuronCount = 250;
    neurons.reserve(neuronCount);

    for (int i = 0; i < neuronCount; ++i) {
        Neuron neuron;
        neuron.id = i;
        neuron.pos = vec3(randRange(-1.0f, 1.0f), randRange(-1.0f, 1.0f), randRange(-1.0f, 1.0f));

        if (i < 80) {
            neuron.section = BrainSection::Sensory;
            neuron.active = (i == 0) ? 1.0f : 0.0f;
        } else if (i < 190) {
            neuron.section = BrainSection::Association;
        } else {
            neuron.section = BrainSection::Output;
            neuron.active = 0.2f;
            for (int slot = 0; slot < outputSlotCount; ++slot) {
                neuron.outputDrive[slot] = randRange(0.0f, 0.1f);
            }
        }

        neurons.push_back(neuron);
    }

    for (int i = 0; i < neuronCount; ++i) {
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

float standingReward(const Ragdoll& ragdoll) {
    const Bone& pelvis = ragdoll.bones[0];
    const Bone& torso = ragdoll.bones[1];
    const Bone& leftFoot = ragdoll.bones[8];
    const Bone& rightFoot = ragdoll.bones[10];

    float height = std::max(0.0f, pelvis.pos.y);
    float upright = std::max(0.0f, torso.pos.y - pelvis.pos.y);
    float stillness = 1.0f - std::min(1.0f, length(pelvis.vel) * 0.35f);
    float feetNearGround = 1.0f - std::min(1.0f, 0.5f * (std::fabs(leftFoot.pos.y) + std::fabs(rightFoot.pos.y)));
    float centered = 1.0f - std::min(1.0f, std::fabs(pelvis.pos.x) + std::fabs(pelvis.pos.z));

    float reward = 0.28f * height + 0.25f * upright + 0.2f * stillness + 0.15f * feetNearGround + 0.12f * centered;

    if (pelvis.pos.y < 0.15f) {
        reward -= 0.25f;
    }

    return reward;
}

BodyState standingTarget(const Ragdoll& ragdoll) {
    const Bone& pelvis = ragdoll.bones[0];
    const Bone& torso = ragdoll.bones[1];
    const Bone& leftUpperArm = ragdoll.bones[3];
    const Bone& rightUpperArm = ragdoll.bones[5];
    const Bone& leftUpperLeg = ragdoll.bones[7];
    const Bone& rightUpperLeg = ragdoll.bones[9];
    const Bone& leftFoot = ragdoll.bones[8];
    const Bone& rightFoot = ragdoll.bones[10];

    BodyState target;
    target.movement.x = std::clamp(-pelvis.pos.x * 0.9f - pelvis.vel.x * 0.2f, -0.35f, 0.35f);
    target.movement.y = std::clamp((0.95f - pelvis.pos.y) * 0.8f - pelvis.vel.y * 0.1f, -0.35f, 0.35f);
    target.movement.z = std::clamp(-pelvis.pos.z * 0.9f - pelvis.vel.z * 0.2f, -0.35f, 0.35f);

    target.limbDrive[0] = std::clamp((leftUpperArm.pos.y - torso.pos.y) * 1.1f - leftUpperArm.vel.y * 0.1f, -0.35f, 0.35f);
    target.limbDrive[1] = std::clamp((rightUpperArm.pos.y - torso.pos.y) * 1.1f - rightUpperArm.vel.y * 0.1f, -0.35f, 0.35f);
    float leftLegHeight = (leftUpperLeg.pos.y - pelvis.pos.y) * -1.2f - leftUpperLeg.vel.y * 0.1f;
    float rightLegHeight = (rightUpperLeg.pos.y - pelvis.pos.y) * -1.2f - rightUpperLeg.vel.y * 0.1f;
    float leftLegSupport = (leftFoot.pos.y < 0.04f ? 0.18f : -0.05f) + std::clamp((pelvis.pos.x + 0.12f) * 0.65f, -0.2f, 0.2f);
    float rightLegSupport = (rightFoot.pos.y < 0.04f ? 0.18f : -0.05f) + std::clamp((0.12f - pelvis.pos.x) * 0.65f, -0.2f, 0.2f);
    target.limbDrive[2] = std::clamp(leftLegHeight + leftLegSupport, -0.35f, 0.35f);
    target.limbDrive[3] = std::clamp(rightLegHeight + rightLegSupport, -0.35f, 0.35f);

    target.speech = 0.0f;
    target.expression = std::clamp(0.25f + std::max(0.0f, pelvis.pos.y - 0.2f) * 0.1f + 0.05f * std::fabs(leftLegHeight - rightLegHeight), 0.0f, 0.35f);
    return target;
}

bool shouldResetGeneration(const Ragdoll& ragdoll, float reward, int generationFrame) {
    const Bone& pelvis = ragdoll.bones[0];
    const Bone& torso = ragdoll.bones[1];

    bool collapsed = pelvis.pos.y < 0.18f || torso.pos.y < pelvis.pos.y + 0.08f;
    bool persistentFailure = generationFrame > 90 && reward < 0.16f;
    bool longLowStance = generationFrame > 240 && pelvis.pos.y < 0.35f;

    return (generationFrame > 45 && collapsed) || persistentFailure || longLowStance;
}

bool stepLearningLoop(int globalFrame, int generationFrame, int generation, Body& body, Ragdoll& ragdoll) {
    inject_data(sensory_data(globalFrame, ragdoll, body));
    propagate();
    adapt();
    decay_sense_drive();
    grow();
    prune();

    BodyCommand command = collect_body_output();
    body.apply(command);

    // No standing assist during normal control. The ragdoll starts upright,
    // then the neurons control it directly and must learn to stay balanced.
    float assist = 0.0f;
    ragdoll.step(0.016f, body.state, assist);

    float reward = standingReward(ragdoll);
    BodyState target = standingTarget(ragdoll);
    body.teach(target, 0.02f);
    learningSignal = std::clamp(reward * 1.5f, -1.0f, 1.5f);
    outputNoiseScale = std::clamp(0.02f + (1.0f - std::clamp(reward, 0.0f, 1.0f)) * 0.05f, 0.01f, 0.08f);
    body.learn(reward, 0.01f);

    std::vector<Signal> feedback = body.feedbackSignals();

    Signal standingSignal;
    standingSignal.sense = SenseType::Vision;
    standingSignal.section = BrainSection::Association;
    standingSignal.pos = ragdoll.bones[0].pos;
    standingSignal.value = reward;
    standingSignal.intensity = 0.2f;
    feedback.push_back(standingSignal);

    Signal balanceSignal;
    balanceSignal.sense = SenseType::Touch;
    balanceSignal.section = BrainSection::Sensory;
    balanceSignal.pos = ragdoll.bones[8].pos;
    balanceSignal.value = std::max(0.0f, ragdoll.bones[0].pos.y);
    balanceSignal.intensity = 0.12f;
    feedback.push_back(balanceSignal);

    // Limb proprioception: teach the neurons which outputs move which limbs.
    Signal leftArmSignal;
    leftArmSignal.sense = SenseType::Touch;
    leftArmSignal.section = BrainSection::Association;
    leftArmSignal.pos = ragdoll.bones[4].pos;
    leftArmSignal.value = ragdoll.bones[4].pos.y - ragdoll.bones[3].pos.y;
    leftArmSignal.intensity = 0.1f;
    feedback.push_back(leftArmSignal);

    Signal rightArmSignal = leftArmSignal;
    rightArmSignal.pos = ragdoll.bones[6].pos;
    rightArmSignal.value = ragdoll.bones[6].pos.y - ragdoll.bones[5].pos.y;
    feedback.push_back(rightArmSignal);

    Signal leftLegSignal = leftArmSignal;
    leftLegSignal.pos = ragdoll.bones[8].pos;
    leftLegSignal.value = ragdoll.bones[8].pos.y - ragdoll.bones[7].pos.y;
    feedback.push_back(leftLegSignal);

    Signal rightLegSignal = leftArmSignal;
    rightLegSignal.pos = ragdoll.bones[10].pos;
    rightLegSignal.value = ragdoll.bones[10].pos.y - ragdoll.bones[9].pos.y;
    feedback.push_back(rightLegSignal);

    Signal leftFootSupport;
    leftFootSupport.sense = SenseType::Touch;
    leftFootSupport.section = BrainSection::Sensory;
    leftFootSupport.pos = ragdoll.bones[8].pos;
    leftFootSupport.value = std::max(0.0f, 0.05f - ragdoll.bones[8].pos.y) + std::fabs(ragdoll.bones[0].pos.x + 0.12f);
    leftFootSupport.intensity = 0.15f;
    feedback.push_back(leftFootSupport);

    Signal rightFootSupport = leftFootSupport;
    rightFootSupport.pos = ragdoll.bones[10].pos;
    rightFootSupport.value = std::max(0.0f, 0.05f - ragdoll.bones[10].pos.y) + std::fabs(ragdoll.bones[0].pos.x - 0.12f);
    feedback.push_back(rightFootSupport);

    inject_data(feedback);

    bool failed = shouldResetGeneration(ragdoll, reward, generationFrame);
    if (failed) {
        // terminal penalty for the last generation so the network sees the failure boundary
        body.learn(-0.25f, 0.02f);
        learningSignal = -0.75f;
        outputNoiseScale = 0.18f;
        mutate_generation(0.08f + 0.015f * std::min(generation, 12));
        body.mutate(0.04f + 0.01f * std::min(generation, 10));

        Signal failureSignal;
        failureSignal.sense = SenseType::Touch;
        failureSignal.section = BrainSection::Association;
        failureSignal.pos = ragdoll.bones[0].pos;
        failureSignal.value = -0.25f;
        failureSignal.intensity = 0.35f;
        inject_data({ failureSignal });

        std::cout << "generation " << generation << " failed after " << generationFrame
                  << " frames, resetting to a fresh standing start\n";
    }

    if ((globalFrame % 30) == 0) {
        std::cout << "gen=" << generation
                  << " genFrame=" << generationFrame
                  << " reward=" << reward
                  << " pelvis=" << ragdoll.bones[0].pos.y
                  << " movement=" << body.state.movement.x << "," << body.state.movement.y << "," << body.state.movement.z
                  << " limbs=" << body.state.limbDrive[0] << "," << body.state.limbDrive[1] << "," << body.state.limbDrive[2] << "," << body.state.limbDrive[3]
                  << " speech=" << body.state.speech
                  << " expression=" << body.state.expression
                  << '\n';
    }

    return failed;
}

static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (mouseDown) {
        double dx = xpos - lastX;
        double dy = ypos - lastY;
        camera.az += static_cast<float>(dx) * 0.005f;
        camera.el += static_cast<float>(-dy) * 0.005f;
        if (camera.el > 1.4f) camera.el = 1.4f;
        if (camera.el < -1.4f) camera.el = -1.4f;
    }
    lastX = xpos; lastY = ypos;
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        mouseDown = (action == GLFW_PRESS);
        double x, y; glfwGetCursorPos(window, &x, &y); lastX = x; lastY = y;
    }
}

static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    (void)xoffset;

    // Positive scroll zooms in, negative scroll zooms out.
    camera.dist *= (1.0f - static_cast<float>(yoffset) * 0.1f);
    if (camera.dist < 0.6f) camera.dist = 0.6f;
    if (camera.dist > 20.0f) camera.dist = 20.0f;
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    GLFWwindow* window = glfwCreateWindow(1024, 720, "Ragdoll Viewer", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);

    setupGL();

    Body body;
    Ragdoll ragdoll;
    setupBrain();

    bool paused = false;
    bool pauseKeyWasDown = false;
    int frame = 0;
    int generation = 1;
    int generationFrame = 0;

    while (!glfwWindowShouldClose(window)) {
        if (!paused) {
            bool failed = stepLearningLoop(frame, generationFrame, generation, body, ragdoll);
            ++frame;

            if (failed) {
                ++generation;
                generationFrame = 0;
                ragdoll = Ragdoll();
                body.state = BodyState();
                body.lastCommand = BodyCommand();
                outputNoiseScale = 0.12f;
                continue;
            }

            ++generationFrame;
        }

        setViewport(window);
        beginFrame(camera);

        drawGrid();
        drawRagdoll(ragdoll);

        glfwSwapBuffers(window);
        glfwPollEvents();

        // keyboard
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.target.z -= 0.02f;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.target.z += 0.02f;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.target.x -= 0.02f;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.target.x += 0.02f;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) camera.dist *= 1.01f;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) camera.dist *= 0.99f;
        if (camera.dist < 0.6f) camera.dist = 0.6f;
        if (camera.dist > 20.0f) camera.dist = 20.0f;
        bool pauseKeyDown = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (pauseKeyDown && !pauseKeyWasDown) {
            paused = !paused;
        }
        pauseKeyWasDown = pauseKeyDown;

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
