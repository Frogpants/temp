#pragma once

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <gl/GL.h>

#include <array>
#include <utility>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "opengl32.lib")

namespace window {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Vertex {
    Vec3 position;
    Vec4 color;
    Vec2 texCoord;
};

struct Mat4 {
    std::array<float, 16> value = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
};

class Window {
public:
    Window() = default;

    Window(int width, int height, const char* title) {
        create(width, height, title);
    }

    ~Window() {
        shutdown();
    }

    bool create(int width, int height, const char* title) {
        width_ = width;
        height_ = height;

        HINSTANCE instance = GetModuleHandleA(nullptr);
        if (!registerClass(instance)) {
            return false;
        }

        hwnd_ = CreateWindowExA(
            0,
            className(),
            title,
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            width,
            height,
            nullptr,
            nullptr,
            instance,
            this);

        if (!hwnd_) {
            return false;
        }

        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);

        hdc_ = GetDC(hwnd_);
        if (!setupPixelFormat()) {
            return false;
        }

        hglrc_ = wglCreateContext(hdc_);
        if (!hglrc_ || !wglMakeCurrent(hdc_, hglrc_)) {
            return false;
        }

        glViewport(0, 0, width_, height_);
        return true;
    }

    bool shouldClose() const {
        return closed_;
    }

    Vec2 getMousePosition() const {
        return mousePosition_;
    }

    void pumpEvents() {
        MSG message{};
        while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }

    void clear(float r, float g, float b, float a) {
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void setViewport(int width, int height) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width_, height_);
    }

    bool setShaders(std::string vertexSource, std::string fragmentSource) {
        vertexSource_ = std::move(vertexSource);
        fragmentSource_ = std::move(fragmentSource);

        if (!compileShaders()) {
            return false;
        }

        return true;
    }

    void use() const {
        if (program_) {
            glUseProgram(program_);
        }
    }

    void setUniform(std::string_view name, float value) {
        GLint location = uniformLocation(name);
        if (location >= 0) {
            glUniform1f(location, value);
        }
    }

    void setUniform(std::string_view name, int value) {
        GLint location = uniformLocation(name);
        if (location >= 0) {
            glUniform1i(location, value);
        }
    }

    void setUniform(std::string_view name, const Vec2& value) {
        GLint location = uniformLocation(name);
        if (location >= 0) {
            glUniform2f(location, value.x, value.y);
        }
    }

    void setUniform(std::string_view name, const Vec3& value) {
        GLint location = uniformLocation(name);
        if (location >= 0) {
            glUniform3f(location, value.x, value.y, value.z);
        }
    }

    void setUniform(std::string_view name, const Vec4& value) {
        GLint location = uniformLocation(name);
        if (location >= 0) {
            glUniform4f(location, value.x, value.y, value.z, value.w);
        }
    }

    void setUniform(std::string_view name, const Mat4& value) {
        GLint location = uniformLocation(name);
        if (location >= 0) {
            glUniformMatrix4fv(location, 1, GL_FALSE, value.value.data());
        }
    }

    void drawTriangles(const std::array<Vertex, 3>& vertices) {
        use();

        glBegin(GL_TRIANGLES);
        for (const Vertex& vertex : vertices) {
            glColor4f(vertex.color.x, vertex.color.y, vertex.color.z, vertex.color.w);
            glTexCoord2f(vertex.texCoord.x, vertex.texCoord.y);
            glVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
        }
        glEnd();
    }

    void present() {
        SwapBuffers(hdc_);
    }

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        Window* self = nullptr;
        if (message == WM_NCCREATE) {
            CREATESTRUCTA* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
            self = reinterpret_cast<Window*>(create->lpCreateParams);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->hwnd_ = hwnd;
        } else {
            self = reinterpret_cast<Window*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
        }

        if (self) {
            switch (message) {
                case WM_CLOSE:
                case WM_DESTROY:
                    self->closed_ = true;
                    PostQuitMessage(0);
                    return 0;
                case WM_SIZE:
                    self->setViewport(LOWORD(lParam), HIWORD(lParam));
                    return 0;
                case WM_MOUSEMOVE:
                    self->mousePosition_.x = static_cast<float>(GET_X_LPARAM(lParam));
                    self->mousePosition_.y = static_cast<float>(GET_Y_LPARAM(lParam));
                    return 0;
                default:
                    break;
            }
        }

        return DefWindowProcA(hwnd, message, wParam, lParam);
    }

private:
    static const char* className() {
        return "window.hpp.opengl.window";
    }

    bool registerClass(HINSTANCE instance) {
        WNDCLASSA wc{};
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = &Window::windowProc;
        wc.hInstance = instance;
        wc.lpszClassName = className();
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        return RegisterClassA(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }

    bool setupPixelFormat() {
        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int pixelFormat = ChoosePixelFormat(hdc_, &pfd);
        if (pixelFormat == 0) {
            return false;
        }

        return SetPixelFormat(hdc_, pixelFormat, &pfd) != FALSE;
    }

    bool compileShaders() {
        if (!vertexSource_.empty()) {
            GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
            const char* source = vertexSource_.c_str();
            glShaderSource(vertexShader, 1, &source, nullptr);
            glCompileShader(vertexShader);

            if (!checkShader(vertexShader)) {
                glDeleteShader(vertexShader);
                return false;
            }

            vertexShader_ = vertexShader;
        }

        if (!fragmentSource_.empty()) {
            GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
            const char* source = fragmentSource_.c_str();
            glShaderSource(fragmentShader, 1, &source, nullptr);
            glCompileShader(fragmentShader);

            if (!checkShader(fragmentShader)) {
                glDeleteShader(fragmentShader);
                return false;
            }

            fragmentShader_ = fragmentShader;
        }

        if (program_) {
            glDeleteProgram(program_);
            program_ = 0;
        }

        program_ = glCreateProgram();
        if (vertexShader_) {
            glAttachShader(program_, vertexShader_);
        }
        if (fragmentShader_) {
            glAttachShader(program_, fragmentShader_);
        }

        glLinkProgram(program_);
        if (!checkProgram(program_)) {
            glDeleteProgram(program_);
            program_ = 0;
            return false;
        }

        uniformLocations_.clear();
        return true;
    }

    bool checkShader(GLuint shader) const {
        GLint status = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        return status == GL_TRUE;
    }

    bool checkProgram(GLuint program) const {
        GLint status = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        return status == GL_TRUE;
    }

    GLint uniformLocation(std::string_view name) {
        auto found = uniformLocations_.find(std::string(name));
        if (found != uniformLocations_.end()) {
            return found->second;
        }

        GLint location = glGetUniformLocation(program_, std::string(name).c_str());
        uniformLocations_.emplace(std::string(name), location);
        return location;
    }

    void shutdown() {
        if (program_) {
            glDeleteProgram(program_);
            program_ = 0;
        }
        if (hglrc_) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(hglrc_);
            hglrc_ = nullptr;
        }
        if (hwnd_ && hdc_) {
            ReleaseDC(hwnd_, hdc_);
            hdc_ = nullptr;
        }
        if (hwnd_) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
    }

    HWND hwnd_ = nullptr;
    HDC hdc_ = nullptr;
    HGLRC hglrc_ = nullptr;
    bool closed_ = false;
    int width_ = 0;
    int height_ = 0;
    Vec2 mousePosition_ = {};
    GLuint program_ = 0;
    GLuint vertexShader_ = 0;
    GLuint fragmentShader_ = 0;
    std::string vertexSource_;
    std::string fragmentSource_;
    std::unordered_map<std::string, GLint> uniformLocations_;
};

} // namespace window

#else

#include <array>
#include <string>
#include <string_view>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/gl.h>

namespace window {

struct Vec2 { float x = 0.0f; float y = 0.0f; };
struct Vec3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };
struct Vec4 { float x = 0.0f; float y = 0.0f; float z = 0.0f; float w = 1.0f; };

struct Vertex {
    Vec3 position;
    Vec4 color;
    Vec2 texCoord;
};

struct Mat4 {
    std::array<float, 16> value = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
};

class Window {
public:
    Window() = default;

    Window(int width, int height, const char* title) {
        create(width, height, title);
    }

    ~Window() {
        shutdown();
    }

    bool create(int width, int height, const char* title) {
        width_ = width;
        height_ = height;

        if (!glfwInit()) {
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

        window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!window_) {
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(window_);
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, &Window::framebufferSizeCallback);
        glfwSetCursorPosCallback(window_, &Window::cursorPosCallback);

        glViewport(0, 0, width_, height_);
        glfwSwapInterval(1);
        return true;
    }

    bool shouldClose() const {
        return window_ == nullptr || glfwWindowShouldClose(window_);
    }

    Vec2 getMousePosition() const {
        return mousePosition_; 
    }

    void pumpEvents() {
        glfwPollEvents();
    }

    void clear(float r, float g, float b, float a) {
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void setViewport(int width, int height) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width_, height_);
    }

    bool setShaders(std::string vertexSource, std::string fragmentSource) {
        vertexSource_ = std::move(vertexSource);
        fragmentSource_ = std::move(fragmentSource);
        return true;
    }

    void use() const {}

    void setUniform(std::string_view, float) {}
    void setUniform(std::string_view, int) {}
    void setUniform(std::string_view, const Vec2&) {}
    void setUniform(std::string_view, const Vec3&) {}
    void setUniform(std::string_view, const Vec4&) {}
    void setUniform(std::string_view, const Mat4&) {}

    void drawTriangles(const std::array<Vertex, 3>& vertices) {
        glBegin(GL_TRIANGLES);
        for (const Vertex& vertex : vertices) {
            glColor4f(vertex.color.x, vertex.color.y, vertex.color.z, vertex.color.w);
            glTexCoord2f(vertex.texCoord.x, vertex.texCoord.y);
            glVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
        }
        glEnd();
    }

    void present() {
        if (window_) {
            glfwSwapBuffers(window_);
        }
    }

private:
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self) {
            self->setViewport(width, height);
        }
    }

    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self) {
            self->mousePosition_.x = static_cast<float>(xpos);
            self->mousePosition_.y = static_cast<float>(ypos);
        }
    }

    void shutdown() {
        if (window_) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }
        glfwTerminate();
    }

    GLFWwindow* window_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    Vec2 mousePosition_ = {};
    std::string vertexSource_;
    std::string fragmentSource_;
};

} // namespace window

#endif

using Window = window::Window;