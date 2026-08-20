/**
 * @file OakularApp.cpp
 * @brief Oakular standalone application - window, file browser and quit flow.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/BlackThorn
 */

// Must be defined before including imgui.h (through OakularApp.hpp)
#define IMGUI_DEFINE_MATH_OPERATORS

#include "OakularApp.hpp"

#include <ImGuiFileDialog.h>

#include <memory>

namespace oakular {

//! \brief Key of the ImGuiFileDialog instance loading a behavior tree.
static constexpr char const* c_load_dialog_key = "LoadYamlDlgKey";
//! \brief Key of the ImGuiFileDialog instance saving a behavior tree.
static constexpr char const* c_save_dialog_key = "SaveYamlDlgKey";

// ----------------------------------------------------------------------------
OakularApp::OakularApp(size_t const p_width, size_t const p_height)
    : Application(p_width, p_height)
{
}

// ----------------------------------------------------------------------------
bool OakularApp::onSetup()
{
    setTitle("Oakular - BlackThorn Editor");

    // The editor cannot browse the file system nor close the window: it asks
    // and this application answers.
    m_editor.onFileDialogRequested.connect(
        [this](Editor::FileDialog const p_dialog) {
            openFileDialog(p_dialog);
        });
    m_editor.onQuitRequested.connect([this]() { requestQuit(); });

#if defined(OAKULAR_HAS_SERVER)
    m_editor.attachServer(std::make_shared<Server>());
#endif

    return m_editor.setup();
}

// ----------------------------------------------------------------------------
void OakularApp::onTeardown()
{
    m_editor.teardown();
}

// ----------------------------------------------------------------------------
void OakularApp::onUpdate(float const p_dt)
{
    m_editor.update(p_dt);
}

// ----------------------------------------------------------------------------
void OakularApp::onDrawMenuBar()
{
    m_editor.drawMenuBar();
}

// ----------------------------------------------------------------------------
void OakularApp::onDrawMainPanel()
{
    m_editor.draw("Behavior Tree");

    showFileDialogs();
    showQuitConfirmationPopup();
}

// ----------------------------------------------------------------------------
void OakularApp::openFileDialog(Editor::FileDialog const p_dialog) const
{
    IGFD::FileDialogConfig config;
    config.path = ".";
    config.countSelectionMax = 1;

    if (p_dialog == Editor::FileDialog::Load)
    {
        config.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog(
            c_load_dialog_key, "Choose YAML File", ".yaml,.yml", config);
        return;
    }

    config.flags =
        ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ConfirmOverwrite;
    ImGuiFileDialog::Instance()->OpenDialog(
        c_save_dialog_key, "Save YAML File", ".yaml", config);
}

// ----------------------------------------------------------------------------
void OakularApp::showFileDialogs()
{
    if (ImGuiFileDialog::Instance()->Display(c_load_dialog_key))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            m_editor.loadFromYaml(
                ImGuiFileDialog::Instance()->GetFilePathName());
        }

        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display(c_save_dialog_key))
    {
        bool saved = false;
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            saved = m_editor.saveToYaml(
                ImGuiFileDialog::Instance()->GetFilePathName());
        }

        ImGuiFileDialog::Instance()->Close();

        // Only leave once the tree has actually reached the disk.
        if (m_quit_after_save)
        {
            m_quit_after_save = false;
            if (saved)
            {
                halt();
            }
        }
    }
}

// ----------------------------------------------------------------------------
bool OakularApp::onCloseRequested()
{
    if (!m_editor.isModified())
    {
        return true;
    }

    m_show_quit_confirmation = true;
    return false;
}

// ----------------------------------------------------------------------------
void OakularApp::requestQuit()
{
    if (onCloseRequested())
    {
        halt();
    }
}

// ----------------------------------------------------------------------------
void OakularApp::saveThenQuit()
{
    if (m_editor.filepath().empty())
    {
        // Never saved yet: the window closes from showFileDialogs(), once the
        // user picked a destination and the tree reached the disk.
        m_quit_after_save = true;
        openFileDialog(Editor::FileDialog::Save);
        return;
    }

    if (m_editor.save())
    {
        halt();
    }
}

// ----------------------------------------------------------------------------
void OakularApp::showQuitConfirmationPopup()
{
    if (m_show_quit_confirmation)
    {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(
            center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Confirm Quit");
        m_show_quit_confirmation = false;
    }

    if (ImGui::BeginPopupModal(
            "Confirm Quit", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("You have unsaved changes. Are you sure you want to quit?");
        ImGui::Separator();

        if (ImGui::Button("Save and Quit", ImVec2(120, 0)))
        {
            saveThenQuit();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Quit Without Saving", ImVec2(160, 0)))
        {
            halt();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace oakular
