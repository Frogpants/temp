#pragma once

// Minimal OpenGL renderer using GLFW for windowing and input.
// Draws a skeleton (lines between bones) and joints (points). Camera orbit + pan.

#include <vector>
#include <iostream>

#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include "ragdoll.hpp"

struct Camera {
    float az = 0.0f; // yaw
    float el = 0.3f; // elevation
    float dist = 3.0f;
    vec3 target = vec3(0.0f, 1.0f, 0.0f);
};

inline void setupGL() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POINT_SMOOTH);
    glPointSize(8.0f);
    glLineWidth(3.0f);
}

inline void setViewport(GLFWwindow* window) {
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (h > 0) ? (float)w / (float)h : 1.0f;
    gluPerspective(60.0, aspect, 0.01, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

inline void beginFrame(const Camera& cam) {
    glClearColor(0.1f, 0.12f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // compute camera position
    float cx = cam.target.x + cam.dist * cosf(cam.el) * sinf(cam.az);
    float cy = cam.target.y + cam.dist * sinf(cam.el);
    float cz = cam.target.z + cam.dist * cosf(cam.el) * cosf(cam.az);

    gluLookAt(cx, cy, cz, cam.target.x, cam.target.y, cam.target.z, 0.0f, 1.0f, 0.0f);
}

inline void drawRagdoll(const Ragdoll& rag) {
    // draw bones as lines
    glColor3f(0.8f, 0.8f, 0.8f);
    glBegin(GL_LINES);
    for (const auto &b : rag.bones) {
        if (b.parent >= 0) {
            const Bone &p = rag.bones[b.parent];
            glVertex3f(p.pos.x, p.pos.y, p.pos.z);
            glVertex3f(b.pos.x, b.pos.y, b.pos.z);
        }
    }
    glEnd();

    // draw joints
    glColor3f(0.9f, 0.4f, 0.3f);
    glBegin(GL_POINTS);
    for (const auto &b : rag.bones) {
        glVertex3f(b.pos.x, b.pos.y, b.pos.z);
    }
    glEnd();
}

inline void drawGrid() {
    glColor3f(0.25f, 0.25f, 0.25f);
    glBegin(GL_LINES);
    for (int i = -10; i <= 10; ++i) {
        glVertex3f(i * 0.2f, 0.0f, -2.0f);
        glVertex3f(i * 0.2f, 0.0f, 2.0f);
        glVertex3f(-2.0f, 0.0f, i * 0.2f);
        glVertex3f(2.0f, 0.0f, i * 0.2f);
    }
    glEnd();
}
