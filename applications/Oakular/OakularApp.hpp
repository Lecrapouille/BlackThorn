/**
 * @file OakularApp.hpp
 * @brief Oakular standalone application - Hosts the Oakular editor.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/BlackThorn
 */

#pragma once

#include "Application/Application.hpp"
#include "Oakular/Oakular.hpp"

namespace oakular {

// ****************************************************************************
//! \brief Standalone application hosting the Oakular behavior tree editor.
//!
//! This class owns what the editor library deliberately does not: the GLFW
//! window, the Dear ImGui context, the file browser and the quit confirmation.
//! The editor itself is a plain member: it is composed, not inherited. Any
//! other application embedding Oakular follows the same pattern.
// ****************************************************************************
class OakularApp: public Application
{
public:

    // ------------------------------------------------------------------------
    //! \brief Constructor.
    //! \param p_width Initial framebuffer width.
    //! \param p_height Initial framebuffer height.
    // ------------------------------------------------------------------------
    explicit OakularApp(size_t const p_width, size_t const p_height);

    // ------------------------------------------------------------------------
    //! \brief Destructor.
    // ------------------------------------------------------------------------
    virtual ~OakularApp() = default;

private: // Overrides from Application

    bool onSetup() override;
    void onTeardown() override;
    void onUpdate(float const p_dt) override;
    void onDrawMenuBar() override;
    void onDrawMainPanel() override;

private: // Application-owned widgets

    //! \brief Open the file browser the editor asked for.
    //! \param p_dialog Whether to load or to save.
    void openFileDialog(Editor::FileDialog const p_dialog) const;

    //! \brief Draw the load and save file browsers.
    void showFileDialogs();

    //! \brief Draw the confirmation asked before losing unsaved changes.
    void showQuitConfirmationPopup();

    //! \brief Close the window, asking for confirmation when relevant.
    void requestQuit();

private:

    //! \brief The editor, embedded exactly like any other host would.
    Editor m_editor;
    //! \brief Flag to open the quit confirmation popup.
    bool m_show_quit_confirmation = false;
    //! \brief Whether the pending save shall be followed by a quit.
    bool m_quit_after_save = false;
};

} // namespace oakular
