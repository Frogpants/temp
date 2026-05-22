#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

#include "essentials.hpp"
#include "body.hpp"

// Lightweight ragdoll: point-mass bones connected by distance constraints
// and simple muscle forces driven by the BodyState. This is not a full
// rigid-body physics engine — it's a compact, header-only approximation
// suitable for emergent control experiments and fast iterations.

struct Bone {
    int id = -1;
    int parent = -1; // -1 for root
    vec3 pos = vec3(0.0f);
    vec3 homePos = vec3(0.0f);
    vec3 prevPos = vec3(0.0f);
    vec3 vel = vec3(0.0f);
    float mass = 1.0f;
    float invMass = 1.0f;
    float radius = 0.02f;
    float restLength = 0.1f;
    float stiffness = 150.0f;
    float damping = 4.0f;
    vec3 restDir = vec3(0.0f); // initial local direction from parent
    float minAngle = -3.14159f;
    float maxAngle = 3.14159f;
};

struct ContactEvent {
    vec3 position = vec3(0.0f);
    vec3 normal = vec3(0.0f, 1.0f, 0.0f);
    float strength = 0.0f;
    int boneA = -1;
    int boneB = -1;
};

struct Ragdoll {
    std::vector<Bone> bones;
    std::vector<ContactEvent> contacts;
    vec3 gravity = vec3(0.0f, -9.81f, 0.0f);

    Ragdoll() {
        buildSimpleHumanoid();
    }

    void buildSimpleHumanoid() {
        bones.clear();

        // Root (pelvis)
        addBone(-1, vec3(0.0f, 0.98f, 0.0f), 2.0f);
        // Torso
        addBone(0, vec3(0.0f, 1.22f, 0.0f), 1.5f);
        // Head
        addBone(1, vec3(0.0f, 1.46f, 0.0f), 0.8f);

        // Left arm (upper, lower)
        addBone(1, vec3(-0.25f, 1.32f, 0.0f), 0.8f);
        addBone(3, vec3(-0.58f, 1.16f, 0.0f), 0.6f);
        // Right arm
        addBone(1, vec3(0.25f, 1.32f, 0.0f), 0.8f);
        addBone(5, vec3(0.58f, 1.16f, 0.0f), 0.6f);

        // Left leg
        addBone(0, vec3(-0.12f, 0.52f, 0.0f), 1.6f);
        addBone(7, vec3(-0.12f, 0.0f, 0.0f), 1.2f);
        // Right leg
        addBone(0, vec3(0.12f, 0.52f, 0.0f), 1.6f);
        addBone(9, vec3(0.12f, 0.0f, 0.0f), 1.2f);

        // compute rest lengths and invMass
        for (auto &b : bones) {
            if (b.parent >= 0) {
                vec3 delta = bones[b.parent].pos - b.pos;
                b.restLength = std::max(0.001f, length(delta));
                // rest direction points from parent -> bone (child relative to parent)
                b.restDir = vec3((b.pos.x - bones[b.parent].pos.x), (b.pos.y - bones[b.parent].pos.y), (b.pos.z - bones[b.parent].pos.z));
                // normalize
                float l = length(b.restDir);
                if (l > 1e-6f) {
                    b.restDir = b.restDir / vec3(l);
                }
            }
            b.invMass = (b.mass > 0.0f) ? 1.0f / b.mass : 0.0f;
            b.prevPos = b.pos;
            b.vel = vec3(0.0f);
        }
    }

    int addBone(int parent, const vec3& pos, float mass) {
        Bone b;
        b.id = static_cast<int>(bones.size());
        b.parent = parent;
        b.pos = pos;
        b.homePos = pos;
        b.mass = mass;
        b.invMass = 1.0f / mass;
        bones.push_back(b);
        return b.id;
    }

    // Map BodyState to muscle forces. This is intentionally simple and
    // tunable; movement.x/y/z steer the root/torso while limbDrive values
    // directly bias individual arms and legs.
    void applyMuscles(const BodyState& state, std::vector<vec3>& outForces) {
        outForces.assign(bones.size(), vec3(0.0f));

        if (bones.empty()) return;

        // global movement controls the root and torso balance.
        vec3 sway = state.movement;

        if (bones.size() > 1) {
            outForces[0] = outForces[0] + vec3(sway.x * 1.8f, sway.y * 2.0f, sway.z * 1.8f);
            outForces[1] = outForces[1] + vec3(sway.x * 1.1f, sway.y * 1.5f, sway.z * 1.1f);
        }

        // arms indices: 3 (left upper), 4 (left lower), 5 (right upper), 6 (right lower)
        if (bones.size() > 6) {
            float leftArm = state.limbDrive[0];
            float rightArm = state.limbDrive[1];

            outForces[3] = vec3(-0.8f * leftArm - sway.x * 0.35f, 0.6f * leftArm + sway.y * 0.15f, 0.5f * leftArm + sway.z * 0.15f) * 4.0f;
            outForces[4] = vec3(-0.5f * leftArm - sway.x * 0.2f, 0.3f * leftArm + sway.y * 0.1f, 0.3f * leftArm + sway.z * 0.08f) * 2.8f;
            outForces[5] = vec3(0.8f * rightArm + sway.x * 0.35f, 0.6f * rightArm + sway.y * 0.15f, 0.5f * rightArm + sway.z * 0.15f) * 4.0f;
            outForces[6] = vec3(0.5f * rightArm + sway.x * 0.2f, 0.3f * rightArm + sway.y * 0.1f, 0.3f * rightArm + sway.z * 0.08f) * 2.8f;
        }

        // legs get their own explicit control channels.
        if (bones.size() > 10) {
            float leftLeg = state.limbDrive[2];
            float rightLeg = state.limbDrive[3];

            outForces[7] = vec3(-0.2f * leftLeg + sway.x * 0.2f, -1.4f * leftLeg + sway.y * -0.8f, 0.7f * leftLeg + sway.z * 0.2f) * 3.2f;
            outForces[8] = vec3(-0.1f * leftLeg + sway.x * 0.12f, -0.9f * leftLeg + sway.y * -0.5f, 0.45f * leftLeg + sway.z * 0.14f) * 2.4f;
            outForces[9] = vec3(0.2f * rightLeg - sway.x * 0.2f, -1.4f * rightLeg + sway.y * -0.8f, -0.7f * rightLeg + sway.z * 0.2f) * 3.2f;
            outForces[10] = vec3(0.1f * rightLeg - sway.x * 0.12f, -0.9f * rightLeg + sway.y * -0.5f, -0.45f * rightLeg + sway.z * 0.14f) * 2.4f;
        }

        // torso modulation by speech/expression
        outForces[1] = outForces[1] + vec3(state.speech * 0.5f, state.expression * 0.2f + state.posture * 0.35f, 0.0f);
    }

    void applyStandingAssist(std::vector<vec3>& forces, float assist) {
        if (assist <= 0.0f) return;

        for (size_t i = 0; i < bones.size(); ++i) {
            Bone& b = bones[i];
            vec3 offset = b.homePos - b.pos;
            float gain = b.stiffness * assist;
            float dampingGain = b.damping * assist;

            // Keep the pelvis and torso upright more aggressively than limbs.
            if (b.id == 0 || b.id == 1) {
                gain *= 2.5f;
                dampingGain *= 2.0f;
            }

            forces[i] = forces[i] + offset * vec3(gain) - b.vel * vec3(dampingGain);

            // Keep feet planted gently near the ground plane.
            if (b.id == 8 || b.id == 10) {
                forces[i].y += std::max(0.0f, (0.02f - b.pos.y) * 250.0f * assist);
                forces[i].x -= b.vel.x * 3.0f * assist;
                forces[i].z -= b.vel.z * 3.0f * assist;
            }
        }
    }

    // helper math
    static float dot3(const vec3 &a, const vec3 &b) {
        return a.x*b.x + a.y*b.y + a.z*b.z;
    }

    static vec3 cross3(const vec3 &a, const vec3 &b) {
        return vec3(a.y*b.z - a.z*b.y,
                    a.z*b.x - a.x*b.z,
                    a.x*b.y - a.y*b.x);
    }

    static vec3 normalize3(const vec3 &v) {
        float l = length(v);
        if (l <= 1e-6f) return vec3(0.0f);
        return v / vec3(l);
    }

    static bool finiteVec3(const vec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    void repairInvalidState() {
        for (auto &b : bones) {
            if (!finiteVec3(b.pos)) {
                b.pos = vec3(0.0f, std::max(0.0f, 1.0f - 0.15f * static_cast<float>(b.id)), 0.0f);
            }
            if (!finiteVec3(b.prevPos)) {
                b.prevPos = b.pos;
            }
            if (!finiteVec3(b.vel)) {
                b.vel = vec3(0.0f);
            }
        }
    }

    // rotate vector 'v' around axis by angle (radians) using Rodrigues' formula
    static vec3 rotateAroundAxis(const vec3 &v, const vec3 &axis, float angle) {
        vec3 k = normalize3(axis);
        float c = std::cos(angle);
        float s = std::sin(angle);
        // v*c + (k x v)*s + k*(k.v)*(1-c)
        vec3 term1 = v * vec3(c);
        vec3 term2 = cross3(k, v) * vec3(s);
        vec3 term3 = k * vec3(dot3(k, v) * (1.0f - c));
        return term1 + term2 + term3;
    }

    void integrate(float dt, const std::vector<vec3>& forces) {
        for (size_t i = 0; i < bones.size(); ++i) {
            Bone &b = bones[i];
            // semi-implicit Euler
            vec3 acc = forces[i] * b.invMass + gravity;
            b.vel = b.vel + acc * dt;
            b.vel = b.vel * std::exp(-b.damping * dt);
            b.prevPos = b.pos;
            b.pos = b.pos + b.vel * dt;
        }
    }

    void satisfyConstraints(int iterations = 4) {
        for (int iter = 0; iter < iterations; ++iter) {
            for (auto &b : bones) {
                if (b.parent < 0) continue;
                Bone &p = bones[b.parent];

                vec3 delta = b.pos - p.pos;
                float dist = length(delta);
                if (dist <= 1e-6f) continue;

                float diff = (dist - b.restLength) / dist;
                float w1 = b.invMass;
                float w2 = p.invMass;
                float sum = w1 + w2;
                if (sum <= 0.0f) continue;

                vec3 correction = delta * diff;
                p.pos = p.pos + correction * (w1 / sum);
                b.pos = b.pos - correction * (w2 / sum);

                // ground collision
                for (Bone* targetBone : { &b, &p }) {
                    if (targetBone->pos.y < 0.0f) {
                        float penetration = -targetBone->pos.y;
                        targetBone->pos.y = 0.0f;
                        targetBone->vel.y = 0.0f;
                        // friction on ground
                        targetBone->vel.x *= 0.45f;
                        targetBone->vel.z *= 0.45f;

                        ContactEvent contact;
                        contact.position = targetBone->pos;
                        contact.normal = vec3(0.0f, 1.0f, 0.0f);
                        contact.strength = std::max(0.05f, penetration + std::fabs(targetBone->vel.x) + std::fabs(targetBone->vel.z));
                        contact.boneA = targetBone->id;
                        contacts.push_back(contact);
                    }
                }
                
                // enforce simple joint angular limits around restDir
                // compute current direction from parent->child
                vec3 curDir = b.pos - p.pos;
                float curLen = length(curDir);
                if (curLen > 1e-6f) {
                    vec3 curN = curDir / vec3(curLen);
                    vec3 restN = b.restDir;
                    float cosTheta = dot3(curN, restN);
                    cosTheta = std::max(-1.0f, std::min(1.0f, cosTheta));
                    float angle = std::acos(cosTheta);
                    if (angle < b.minAngle || angle > b.maxAngle) {
                        float targetAngle = std::max(b.minAngle, std::min(b.maxAngle, angle));
                        float deltaAngle = targetAngle - angle;
                        vec3 axis = cross3(curN, restN);
                        float axisLen = length(axis);
                        if (axisLen > 1e-6f) {
                            vec3 newDir = rotateAroundAxis(curN, axis, deltaAngle);
                            b.pos = p.pos + newDir * vec3(b.restLength);
                        }
                    }
                }
            }

            // simple self-collision (sphere-sphere) to avoid bone overlap
            for (size_t i = 0; i < bones.size(); ++i) {
                for (size_t j = i + 1; j < bones.size(); ++j) {
                    Bone &a = bones[i];
                    Bone &c = bones[j];
                    vec3 d = c.pos - a.pos;
                    float dist = length(d);
                    float minDist = a.radius + c.radius;
                    if (dist < 1e-6f) continue;
                    if (dist < minDist) {
                        float overlap = minDist - dist;
                        float wa = a.invMass / (a.invMass + c.invMass);
                        float wc = c.invMass / (a.invMass + c.invMass);
                        vec3 n = d / vec3(dist);
                        a.pos = a.pos - n * vec3(overlap * wa);
                        c.pos = c.pos + n * vec3(overlap * wc);

                        ContactEvent contact;
                        contact.position = a.pos + n * vec3(a.radius + overlap * 0.5f);
                        contact.normal = n;
                        contact.strength = std::max(0.03f, overlap * 8.0f);
                        contact.boneA = static_cast<int>(i);
                        contact.boneB = static_cast<int>(j);
                        contacts.push_back(contact);
                    }
                }
            }
        }
    }

    void step(float dt, const BodyState& bodyState, float assist = 0.0f) {
        if (bones.empty()) return;

        contacts.clear();

        std::vector<vec3> forces(bones.size(), vec3(0.0f));
        applyMuscles(bodyState, forces);
        applyStandingAssist(forces, assist);

        // integrate
        integrate(dt, forces);

        // constraints
        satisfyConstraints(5);

        repairInvalidState();

        // damp velocities from positional changes
        for (auto &b : bones) {
            b.vel = (b.pos - b.prevPos) / dt;
            if (b.pos.y <= 0.0f && b.vel.y < 0.0f) {
                b.vel.y = 0.0f;
            }
        }
    }

    void debugPrint() const {
        if (bones.empty()) return;
        const Bone &root = bones[0];
        std::cout << "Ragdoll root pos: (" << root.pos.x << ", " << root.pos.y << ", " << root.pos.z << ")\n";
    }
};
