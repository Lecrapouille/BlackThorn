/**
 * @file DearImGuiApplication.cpp
 * @brief Dear ImGui application wrapper with docking support.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/Robotik
 */

#include "DearImGuiApplication.hpp"

namespace robotik::renderer {

// ----------------------------------------------------------------------------
DearImGuiApplication::DearImGuiApplication(size_t const p_width,
                                           size_t const p_height)
{
    (void) p_width;
    (void) p_height;
}

// ----------------------------------------------------------------------------
DearImGuiApplication::~DearImGuiApplication()
{
    if (m_initialized)
    {
        teardown();
    }
}

// ----------------------------------------------------------------------------
bool DearImGuiApplication::setup()
{
    if (m_initialized)
    {
        return true;
    }

    // Get GLFW window from current context
    m_window = glfwGetCurrentContext();
    if (!m_window)
    {
        return false;
    }

    // Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Configure ImGui flags
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    if (m_docking_enabled)
    {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }

    // Setup ImGui style
    setupStyle();

    // Initialize backends
    const char* glsl_version = "#version 330";
    if (!ImGui_ImplGlfw_InitForOpenGL(m_window, true))
    {
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init(glsl_version))
    {
        ImGui_ImplGlfw_Shutdown();
        return false;
    }

    m_initialized = true;
    return true;
}

// ----------------------------------------------------------------------------
void DearImGuiApplication::teardown()
{
    if (!m_initialized)
    {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_initialized = false;
    m_window = nullptr;
}

// ----------------------------------------------------------------------------
void DearImGuiApplication::draw()
{
    if (!m_initialized)
    {
        return;
    }

    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Setup dockspace
    if (m_docking_enabled)
    {
        setupDockspace();
    }

    // Draw user-defined panels
    if (m_main_panel_callback)
    {
        m_main_panel_callback();
    }

    if (m_side_panel_callback)
    {
        m_side_panel_callback();
    }

    if (m_status_bar_callback)
    {
        m_status_bar_callback();
    }

    // Finalize ImGui frame
    ImGui::Render();

    // Render to main window
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ----------------------------------------------------------------------------
ImGuiIO& DearImGuiApplication::io()
{
    return ImGui::GetIO();
}

// ----------------------------------------------------------------------------
void DearImGuiApplication::enableDocking(bool p_enable)
{
    m_docking_enabled = p_enable;
    if (m_initialized)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (p_enable)
        {
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        }
        else
        {
            io.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
        }
    }
}

// ----------------------------------------------------------------------------
void DearImGuiApplication::setupStyle() const
{
    ImGui::StyleColorsDark();
}

// ----------------------------------------------------------------------------
void DearImGuiApplication::setupDockspace() const
{
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |=
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    window_flags |= ImGuiWindowFlags_MenuBar;

    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
    {
        window_flags |= ImGuiWindowFlags_NoBackground;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // Menu bar - call user callback if provided
    if (ImGui::BeginMenuBar())
    {
        if (m_menu_bar_callback)
        {
            m_menu_bar_callback();
        }
        ImGui::EndMenuBar();
    }

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    ImGui::End();
}

} // namespace robotik::renderer
