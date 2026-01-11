/**
 * @file DearImGuiApplication.hpp
 * @brief Dear ImGui application wrapper with docking support.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/Robotik
 */

#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <functional>

namespace robotik::renderer {

// ****************************************************************************
//! \brief Dear ImGui application wrapper providing docking support.
// ****************************************************************************
class DearImGuiApplication
{
public:

    using MenuBarCallback = std::function<void()>;
    using MainPanelCallback = std::function<void()>;
    using SidePanelCallback = std::function<void()>;
    using StatusBarCallback = std::function<void()>;

    // ------------------------------------------------------------------------
    //! \brief Constructor.
    //! \param p_width Initial window width.
    //! \param p_height Initial window height.
    // ------------------------------------------------------------------------
    explicit DearImGuiApplication(size_t const p_width, size_t const p_height);

    // ------------------------------------------------------------------------
    //! \brief Destructor.
    // ------------------------------------------------------------------------
    ~DearImGuiApplication();

    // ------------------------------------------------------------------------
    //! \brief Set the menu bar callback.
    //! \param p_callback Function to call for menu bar rendering.
    // ------------------------------------------------------------------------
    void setMenuBarCallback(MenuBarCallback&& p_callback)
    {
        m_menu_bar_callback = std::move(p_callback);
    }

    // ------------------------------------------------------------------------
    //! \brief Set the main panel callback.
    //! \param p_callback Function to call for main panel rendering.
    // ------------------------------------------------------------------------
    void setMainPanelCallback(MainPanelCallback&& p_callback)
    {
        m_main_panel_callback = std::move(p_callback);
    }

    // ------------------------------------------------------------------------
    //! \brief Set the side panel callback.
    //! \param p_callback Function to call for side panel rendering.
    // ------------------------------------------------------------------------
    void setSidePanelCallback(SidePanelCallback&& p_callback)
    {
        m_side_panel_callback = std::move(p_callback);
    }

    // ------------------------------------------------------------------------
    //! \brief Set the status bar callback.
    //! \param p_callback Function to call for status bar rendering.
    // ------------------------------------------------------------------------
    void setStatusBarCallback(StatusBarCallback&& p_callback)
    {
        m_status_bar_callback = std::move(p_callback);
    }

    // ------------------------------------------------------------------------
    //! \brief Initialize ImGui context and backends.
    //! \return true if initialization successful, false otherwise.
    // ------------------------------------------------------------------------
    bool setup();

    // ------------------------------------------------------------------------
    //! \brief Cleanup ImGui context and resources.
    // ------------------------------------------------------------------------
    void teardown();

    // ------------------------------------------------------------------------
    //! \brief Draw the ImGui frame with dockspace.
    // ------------------------------------------------------------------------
    void draw();

    // ------------------------------------------------------------------------
    //! \brief Get ImGui IO structure for configuration.
    //! \return Reference to ImGui IO structure.
    // ------------------------------------------------------------------------
    ImGuiIO& io();

    // ------------------------------------------------------------------------
    //! \brief Enable or disable docking support.
    //! \param p_enable true to enable docking, false to disable.
    // ------------------------------------------------------------------------
    void enableDocking(bool p_enable = true);

private:

    // ------------------------------------------------------------------------
    //! \brief Setup ImGui style and colors.
    // ------------------------------------------------------------------------
    void setupStyle() const;

    // ------------------------------------------------------------------------
    //! \brief Setup and draw the main dockspace.
    // ------------------------------------------------------------------------
    void setupDockspace() const;

private:

    // GLFW window (obtained from current context)
    GLFWwindow* m_window = nullptr;

    // State flags
    bool m_initialized = false;
    bool m_docking_enabled = true;

    // ImGui UI callbacks
    MenuBarCallback m_menu_bar_callback;
    MainPanelCallback m_main_panel_callback;
    SidePanelCallback m_side_panel_callback;
    StatusBarCallback m_status_bar_callback;
};

} // namespace robotik::renderer
