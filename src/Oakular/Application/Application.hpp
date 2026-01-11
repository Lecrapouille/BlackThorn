/**
 * @file Application.hpp
 * @brief Application base class with Dear ImGui integration.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/Robotik
 */

#pragma once

#include "DearImGuiApplication.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <memory>
#include <string>

namespace robotik::renderer {

// ****************************************************************************
//! \brief Base application class with Dear ImGui integration.
//! Provides GLFW window management and Dear ImGui UI.
// ****************************************************************************
class Application
{
public:

    // ------------------------------------------------------------------------
    //! \brief Constructor.
    //! \param p_width Initial window width.
    //! \param p_height Initial window height.
    // ------------------------------------------------------------------------
    explicit Application(size_t const p_width, size_t const p_height);

    // ------------------------------------------------------------------------
    //! \brief Destructor.
    // ------------------------------------------------------------------------
    virtual ~Application();

    // ------------------------------------------------------------------------
    //! \brief Start the application and run the main infinite loop.
    //! This method is blocking and will return when the application halted.
    //! \return true if the application closed successfully, false otherwise.
    // ------------------------------------------------------------------------
    bool run();

    // ------------------------------------------------------------------------
    //! \brief Get the last error message when run() returned false.
    //! \return The error message, or empty string if success.
    // ------------------------------------------------------------------------
    std::string const& error() const;

    // ------------------------------------------------------------------------
    //! \brief Set the title of the application.
    //! \param p_title The new title of the application.
    // ------------------------------------------------------------------------
    void setTitle(std::string const& p_title);

    // ------------------------------------------------------------------------
    //! \brief Get the GLFW window.
    //! \return Pointer to GLFW window.
    // ------------------------------------------------------------------------
    GLFWwindow* window() const;

    // ------------------------------------------------------------------------
    //! \brief Request the application to close.
    // ------------------------------------------------------------------------
    void halt();

    // ------------------------------------------------------------------------
    //! \brief Get ImGui IO structure for configuration.
    //! \return Reference to ImGui IO structure.
    // ------------------------------------------------------------------------
    ImGuiIO& imguiIO();

protected:

    // ------------------------------------------------------------------------
    //! \brief Initialize the application. Called once at startup.
    //! Override this to perform custom initialization.
    //! \return true if initialization was successful, false otherwise.
    // ------------------------------------------------------------------------
    virtual bool onSetup()
    {
        return true;
    }

    // ------------------------------------------------------------------------
    //! \brief Cleanup the application. Called once at shutdown.
    //! Override this to perform custom cleanup.
    // ------------------------------------------------------------------------
    virtual void onTeardown()
    {
        // Default implementation does nothing
    }

    // ------------------------------------------------------------------------
    //! \brief Draw ImGui menu bar. Override to add custom menu items.
    // ------------------------------------------------------------------------
    virtual void onDrawMenuBar()
    {
        // Empty implementation
    }

    // ------------------------------------------------------------------------
    //! \brief Draw ImGui main control panel. Override to add custom controls.
    // ------------------------------------------------------------------------
    virtual void onDrawMainPanel()
    {
        // Empty implementation
    }

    // ------------------------------------------------------------------------
    //! \brief Draw ImGui side panel. Override to add side panel content.
    // ------------------------------------------------------------------------
    virtual void onDrawSidePanel()
    {
        // Empty implementation
    }

    // ------------------------------------------------------------------------
    //! \brief Draw ImGui status bar. Override to add status bar content.
    // ------------------------------------------------------------------------
    virtual void onDrawStatusBar()
    {
        // Empty implementation
    }

    // ------------------------------------------------------------------------
    //! \brief Update the application logic. Called every frame.
    //! \param p_dt Delta time in seconds since last frame.
    // ------------------------------------------------------------------------
    virtual void onUpdate(float const p_dt) = 0;

private:

    // ------------------------------------------------------------------------
    //! \brief Check if the application shall be halted.
    //! \return true if the application shall be halted, false otherwise.
    // ------------------------------------------------------------------------
    bool shallBeHalted() const;

    // ------------------------------------------------------------------------
    //! \brief Initialize the application (GLFW, OpenGL, ImGui).
    //! \return true if initialization was successful, false otherwise.
    // ------------------------------------------------------------------------
    bool setup();

    // ------------------------------------------------------------------------
    //! \brief Cleanup the application.
    // ------------------------------------------------------------------------
    void teardown();

    // ------------------------------------------------------------------------
    //! \brief Render the application frame.
    // ------------------------------------------------------------------------
    void draw();

    // Internal initialization methods
    bool initializeGLFW();
    bool createWindow();
    bool initializeGlew();
    void setupCallbacks();

protected:

    //! \brief The last error message when run() returned false.
    std::string m_error;

private:

    // Window properties
    size_t m_width;
    size_t m_height;
    GLFWwindow* m_window = nullptr;
    std::string m_title;

    // Dear ImGui integration
    std::unique_ptr<DearImGuiApplication> m_imgui_app;
};

} // namespace robotik::renderer
