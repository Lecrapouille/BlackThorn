/**
 * @file Embedded.cpp
 * @brief Embedding the Oakular editor inside a host owning its own window.
 *
 * This example is the non-regression test of the embedding contract: it links
 * against liboakular.a but not against the Application layer of the standalone
 * Oakular application. Everything the editor library refuses to do is done
 * here: creating the GLFW window, creating the Dear ImGui context, driving the
 * frame and, in a real host, browsing the file system.
 *
 * It also shows how a host declares its own domain nodes, here the actions of a
 * robot, so that they appear in the creation palette of the editor.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/BlackThorn
 */

#define IMGUI_DEFINE_MATH_OPERATORS

#include "Oakular/Oakular.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>

namespace {

//! \brief Width of the host window, in pixels.
constexpr int c_window_width = 1280;
//! \brief Height of the host window, in pixels.
constexpr int c_window_height = 720;

// ----------------------------------------------------------------------------
//! \brief Initialize GLEW, tolerating the missing GLX display reported under
//! Wayland: the OpenGL context created by GLFW is usable nonetheless.
//! \return true when OpenGL is usable.
// ----------------------------------------------------------------------------
bool initializeGlew()
{
    glewExperimental = GL_TRUE;
    GLenum const err = glewInit();

    // GLEW leaves spurious errors behind on some drivers.
    while (glGetError() != GL_NO_ERROR)
    {
        // Nothing to do.
    }

    if (err != GLEW_OK && err != GLEW_ERROR_NO_GLX_DISPLAY)
    {
        std::cerr << "Failed to initialize GLEW: "
                  << reinterpret_cast<char const*>(glewGetErrorString(err))
                  << std::endl;
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
//! \brief Draw the full-viewport dockspace the editor windows dock into. This
//! belongs to the host: the editor never lays out the application.
//! \param[in,out] p_editor The embedded editor, for its menus.
// ----------------------------------------------------------------------------
void drawHostDockSpace(oakular::Editor& p_editor)
{
    ImGuiViewport const* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags const flags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("HostDockSpace", nullptr, flags);
    ImGui::PopStyleVar(3);

    if (ImGui::BeginMenuBar())
    {
        // The editor contributes its own menus to the menu bar of the host.
        p_editor.drawMenuBar();
        ImGui::EndMenuBar();
    }

    ImGui::DockSpace(ImGui::GetID("HostDockSpace"));
    ImGui::End();
}

} // namespace

// ----------------------------------------------------------------------------
int main()
{
    // ------------------------------------------------------------------------
    // The host owns the window.
    // ------------------------------------------------------------------------
    if (glfwInit() == GLFW_FALSE)
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(c_window_width,
                                          c_window_height,
                                          "Host embedding Oakular",
                                          nullptr,
                                          nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create the GLFW window" << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!initializeGlew())
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // ------------------------------------------------------------------------
    // The host owns the Dear ImGui context.
    // ------------------------------------------------------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ------------------------------------------------------------------------
    // The editor is a plain member of the host: composed, not inherited. No
    // width, no height, no window.
    // ------------------------------------------------------------------------
    oakular::Editor editor;

    // Domain nodes of the host, offered by the creation palette next to the
    // built-in ones. They are the graphical counterpart of what the host
    // registers in its bt::NodeFactory.
    editor.registerNodeType("MoveToTarget", "Robot");
    editor.registerNodeType("GrabObject", "Robot");
    editor.registerNodeType("BatteryAboveThreshold", "Robot");

    // The editor cannot browse the file system: it asks, the host answers. A
    // real host would open its own file browser here.
    editor.onFileDialogRequested.connect(
        [](oakular::Editor::FileDialog const p_dialog) {
            std::cout << "Editor requested a "
                      << (p_dialog == oakular::Editor::FileDialog::Load
                              ? "load"
                              : "save")
                      << " dialog: the host would open its file browser here."
                      << std::endl;
        });

    // The editor cannot close the window either.
    editor.onQuitRequested.connect([window, &editor]() {
        if (editor.isModified())
        {
            std::cout << "Unsaved changes: a real host would confirm."
                      << std::endl;
        }
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    });

    // ------------------------------------------------------------------------
    // The host owns the frame loop.
    // ------------------------------------------------------------------------
    double previous_time = glfwGetTime();

    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        glfwPollEvents();

        double const now = glfwGetTime();
        auto const dt = static_cast<float>(now - previous_time);
        previous_time = now;

        editor.update(dt);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        drawHostDockSpace(editor);

        // The whole editor, inside a window the host may dock anywhere.
        editor.draw("Behavior Tree Editor");

        ImGui::Render();

        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // ------------------------------------------------------------------------
    // Teardown, in reverse order.
    // ------------------------------------------------------------------------
    editor.teardown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
