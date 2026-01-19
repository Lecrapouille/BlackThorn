/**
 * @file OakularApp.hpp
 * @brief Oakular standalone application - Uses the Oakular library
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "IDE.hpp"

#include <ImGuiFileDialog.h>

// ****************************************************************************
//! \brief Oakular standalone application using the Oakular library.
//!
//! This class provides the UI widgets and window management for the Oakular
//! behavior tree editor. It inherits from IDE which provides the core
//! functionality (menus, tree management, rendering, serialization).
// ****************************************************************************
class OakularApp: public IDE
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

protected:

    // Override from Application
    void onDrawMainPanel() override;

private: // UI widgets

    //! \brief Show the palette for adding new nodes.
    void showAddNodePalette();

    //! \brief Show the context menu for node operations.
    void showNodeContextMenu();

    //! \brief Show the popup for editing node properties.
    void showNodeEditPopup();

    //! \brief Show the quit confirmation popup.
    void showQuitConfirmationPopup();

    //! \brief Show the visualizer panel (TCP server status).
    void showVisualizerPanel();

    //! \brief Show the editor tabs for tree views.
    void showEditorTabs();

    //! \brief Draw a single tree tab.
    //! \param name The name of the tab.
    //! \param view The tree view to draw.
    void drawTreeTab(const std::string& name, TreeView& view);

    //! \brief Show file dialogs (load/save).
    void showFileDialogs();

    //! \brief Handle edit mode interactions (node selection, link creation).
    void handleEditModeInteractions();

    //! \brief Handle keyboard shortcuts.
    void handleKeyboardShortcuts();

    //! \brief Show the blackboard panel.
    void showBlackboardPanel();
};
