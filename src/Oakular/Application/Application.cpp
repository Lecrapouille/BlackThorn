/**
 * @file Application.cpp
 * @brief Application base class with Dear ImGui integration.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/Robotik
 */

#include "Application.hpp"
#include "DearImGuiApplication.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <cstdio>

namespace robotik::renderer {

// ----------------------------------------------------------------------------
Application::Application(size_t const p_width, size_t const p_height)
    : m_width(p_width), m_height(p_height)
{
}

// ----------------------------------------------------------------------------
Application::~Application()
{
    if (m_window)
    {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
}

// ----------------------------------------------------------------------------
std::string const& Application::error() const
{
    return m_error;
}

// ----------------------------------------------------------------------------
void Application::setTitle(std::string const& p_title)
{
    m_title = p_title;
    if (m_window)
    {
        glfwSetWindowTitle(m_window, p_title.c_str());
    }
}

// ----------------------------------------------------------------------------
GLFWwindow* Application::window() const
{
    return m_window;
}

// ----------------------------------------------------------------------------
void Application::halt()
{
    if (m_window)
    {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }
}

// ----------------------------------------------------------------------------
ImGuiIO& Application::imguiIO()
{
    return m_imgui_app->io();
}

// ----------------------------------------------------------------------------
bool Application::shallBeHalted() const
{
    return m_window ? glfwWindowShouldClose(m_window) : true;
}

// ----------------------------------------------------------------------------
bool Application::setup()
{
    // Initialize GLFW
    if (!initializeGLFW())
        return false;

    // Create window
    if (!createWindow())
        return false;

    // Initialize GLEW
    if (!initializeGlew())
        return false;

    // Setup GLFW callbacks
    setupCallbacks();

    // Initialize ImGui
    m_imgui_app = std::make_unique<DearImGuiApplication>(m_width, m_height);
    if (!m_imgui_app->setup())
    {
        m_error = "Failed to initialize ImGui";
        return false;
    }

    // Set the ImGui UI callbacks
    m_imgui_app->setMenuBarCallback([this]() { this->onDrawMenuBar(); });
    m_imgui_app->setMainPanelCallback([this]() { this->onDrawMainPanel(); });
    m_imgui_app->setSidePanelCallback([this]() { this->onDrawSidePanel(); });
    m_imgui_app->setStatusBarCallback([this]() { this->onDrawStatusBar(); });

    return onSetup();
}

// ----------------------------------------------------------------------------
void Application::teardown()
{
    if (m_imgui_app)
    {
        m_imgui_app->teardown();
        m_imgui_app.reset();
    }
    onTeardown();
}

// ----------------------------------------------------------------------------
void Application::draw()
{
    // Poll events first
    glfwPollEvents();

    if (m_imgui_app)
    {
        // Render with ImGui
        m_imgui_app->draw();
    }

    // Swap buffers
    glfwSwapBuffers(m_window);
}

// ----------------------------------------------------------------------------
bool Application::initializeGLFW()
{
    if (!glfwInit())
    {
        m_error = "Failed to initialize GLFW";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    return true;
}

// ----------------------------------------------------------------------------
bool Application::createWindow()
{
    m_window = glfwCreateWindow(
        int(m_width), int(m_height), m_title.c_str(), nullptr, nullptr);
    if (!m_window)
    {
        m_error = "Failed to create GLFW window";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);
    glfwSwapInterval(1); // Enable VSync

    return true;
}

// ----------------------------------------------------------------------------
bool Application::initializeGlew()
{
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();

    while (glGetError() != GL_NO_ERROR)
    {
    }

    if (err != GLEW_OK && err != GLEW_ERROR_NO_GLX_DISPLAY)
    {
        const GLubyte* version = glGetString(GL_VERSION);
        if (version == nullptr)
        {
            m_error = "Failed to initialize GLEW: ";
            m_error += reinterpret_cast<const char*>(glewGetErrorString(err));
            glfwTerminate();
            return false;
        }
    }

    if (!GLEW_VERSION_3_3)
    {
        const char* version =
            reinterpret_cast<const char*>(glGetString(GL_VERSION));
        m_error = "OpenGL 3.3 not supported. Available: ";
        m_error += version ? version : "unknown";
        glfwTerminate();
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
void Application::setupCallbacks()
{
    // Resize callback
    glfwSetFramebufferSizeCallback(
        m_window, [](GLFWwindow* window, int width, int height) {
            (void)window;
            (void)width;
            (void)height;
        });
}

// ----------------------------------------------------------------------------
bool Application::run()
{
    if (!setup())
    {
        return false;
    }

    auto last_time = std::chrono::high_resolution_clock::now();

    // Main application loop
    while (!shallBeHalted())
    {
        auto current_time = std::chrono::high_resolution_clock::now();
        float delta_time =
            std::chrono::duration<float>(current_time - last_time).count();
        last_time = current_time;

        onUpdate(delta_time);
        draw();
    }

    teardown();

    return true;
}

} // namespace robotik::renderer
