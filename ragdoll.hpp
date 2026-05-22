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

struct Ragdoll {
    std::vector<Bone> bones;
    vec3 gravity = vec3(0.0f, -9.81f, 0.0f);

    Ragdoll() {
        buildSimpleHumanoid();
    }

    void buildSimpleHumanoid() {
        bones.clear();

        // Root (pelvis)
        addBone(-1, vec3(0.0f, 1.0f, 0.0f), 2.0f);
        // Torso
        addBone(0, vec3(0.0f, 1.25f, 0.0f), 1.5f);
        // Head
        addBone(1, vec3(0.0f, 1.5f, 0.0f), 0.8f);

        // Left arm (upper, lower)
        addBone(1, vec3(-0.25f, 1.35f, 0.0f), 0.8f);
        addBone(3, vec3(-0.6f, 1.2f, 0.0f), 0.6f);
        // Right arm
        addBone(1, vec3(0.25f, 1.35f, 0.0f), 0.8f);
        addBone(5, vec3(0.6f, 1.2f, 0.0f), 0.6f);

        // Left leg
        addBone(0, vec3(-0.12f, 0.7f, 0.0f), 1.6f);
        addBone(7, vec3(-0.12f, 0.35f, 0.0f), 1.2f);
        // Right leg
        addBone(0, vec3(0.12f, 0.7f, 0.0f), 1.6f);
        addBone(9, vec3(0.12f, 0.35f, 0.0f), 1.2f);

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
        b.mass = mass;
        b.invMass = 1.0f / mass;
        bones.push_back(b);
        return b.id;
    }

    // Map BodyState to muscle forces. This is intentionally simple and
    // tunable; movement.x/y/z steer limbs, speech/expression modulate torso.
    void applyMuscles(const BodyState& state, std::vector<vec3>& outForces) {
        outForces.assign(bones.size(), vec3(0.0f));

        if (bones.empty()) return;

        // lateral sway -> arms
        vec3 sway = state.movement;

        // arms indices: 3 (left upper), 4 (left lower), 5 (right upper), 6 (right lower)
        if (bones.size() > 6) {
            outForces[3] = vec3(-sway.x, sway.y * 0.5f, sway.z) * 3.0f;
            outForces[4] = vec3(-sway.x * 0.6f, sway.y * 0.3f, sway.z * 0.5f) * 2.0f;
            outForces[5] = vec3(-outForces[3].x, outForces[3].y, outForces[3].z);
            outForces[6] = vec3(-outForces[4].x, outForces[4].y, outForces[4].z);
        }

        // legs: use movement.y to drive forward/backwards
        if (bones.size() > 10) {
            outForces[7] = vec3(sway.z * 2.0f, sway.y * -1.5f, sway.x * 0.2f);
            outForces[8] = vec3(sway.z * 1.5f, sway.y * -1.0f, sway.x * 0.15f);
            outForces[9] = vec3(-sway.z * 2.0f, sway.y * -1.5f, -sway.x * 0.2f);
            outForces[10] = vec3(-sway.z * 1.5f, sway.y * -1.0f, -sway.x * 0.15f);
        }

        // torso modulation by speech/expression
        outForces[1] += vec3(state.speech * 0.5f, state.expression * 0.2f, 0.0f);
        outForces[0] += vec3(0.0f, -9.81f * bones[0].mass, 0.0f); // persistent gravity compensation on root
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
                        targetBone->pos.y = 0.0f;
                        targetBone->vel.y *= -0.2f;
                        // friction on ground
                        targetBone->vel.x *= 0.8f;
                        targetBone->vel.z *= 0.8f;
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
                    }
                }
            }
        }
    }

    void step(float dt, const BodyState& bodyState) {
        if (bones.empty()) return;

        std::vector<vec3> forces(bones.size(), vec3(0.0f));
        applyMuscles(bodyState, forces);

        // integrate
        integrate(dt, forces);

        // constraints
        satisfyConstraints(5);

        // damp velocities from positional changes
        for (auto &b : bones) {
            b.vel = (b.pos - b.prevPos) / dt;
        }
    }

    void debugPrint() const {
        if (bones.empty()) return;
        const Bone &root = bones[0];
        std::cout << "Ragdoll root pos: (" << root.pos.x << ", " << root.pos.y << ", " << root.pos.z << ")\n";
    }
};
