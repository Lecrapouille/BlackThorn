/**
 * @file EditorWidgets.cpp
 * @brief Oakular - Embeddable behavior tree editor: Dear ImGui widgets.
 *
 * None of these widgets create a window, an OpenGL context or an ImGui context:
 * they all draw inside the frame owned by the host application.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/BlackThorn
 */

// Must be defined before including imgui.h (through Editor.hpp)
#define IMGUI_DEFINE_MATH_OPERATORS

#include "Editor.hpp"
#include "TreeRenderer.hpp"

#if defined(OAKULAR_HAS_SERVER)
#    include "Server.hpp"
#endif

#include <imgui_stdlib.h>

#include <any>
#include <array>
#include <cstddef>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace oakular {

namespace {

// ----------------------------------------------------------------------------
//! \brief Check whether any ImGui popup or modal is currently open. Used to
//! mute the editor interactions while the user is busy in a dialog, including
//! dialogs owned by the host application.
// ----------------------------------------------------------------------------
bool isAnyPopupOpen()
{
    return ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
}

//! \brief Shorthands for the blackboard storage types drawn by this panel.
using BlackboardValue = bt::Blackboard::Value;
using BlackboardMap = bt::BlackboardMap;
using BlackboardList = std::vector<bt::BlackboardValue>;

// ----------------------------------------------------------------------------
//! \brief Convert a scalar blackboard value to the text of its edit box.
//!
//! Distinct from \c bt::Blackboard::displayValue, which rounds reals for a
//! compact read-only display: the text produced here is what the user edits and
//! types back, so it must round-trip.
// ----------------------------------------------------------------------------
std::string scalarToEditableText(BlackboardValue const& p_value)
{
    auto const& base = p_value.asBase();

    if (auto const* v = std::get_if<int>(&base))
        return std::to_string(*v);
    if (auto const* v = std::get_if<std::size_t>(&base))
        return std::to_string(*v);
    if (auto const* v = std::get_if<bool>(&base))
        return *v ? "true" : "false";
    if (auto const* v = std::get_if<std::string>(&base))
        return *v;
    if (auto const* v = std::get_if<double>(&base))
    {
        std::ostringstream oss;
        oss << *v;
        return oss.str();
    }
    if (auto const* v = std::get_if<float>(&base))
    {
        std::ostringstream oss;
        oss << *v;
        return oss.str();
    }

    return bt::Blackboard::displayValue(p_value);
}

// ----------------------------------------------------------------------------
//! \brief Struct fields of \p p_value, or \c nullptr for any other type.
//! \param[in] p_value Entry to inspect, may be \c nullptr.
// ----------------------------------------------------------------------------
BlackboardMap* asStruct(BlackboardValue* p_value)
{
    return (p_value != nullptr) ? std::get_if<BlackboardMap>(&p_value->asBase())
                                : nullptr;
}

// ----------------------------------------------------------------------------
//! \brief Get the human readable name of the type currently held by a value.
// ----------------------------------------------------------------------------
std::string valueTypeName(BlackboardValue const& p_value)
{
    auto const& base = p_value.asBase();

    if (std::holds_alternative<std::monostate>(base))
        return "null";
    if (std::holds_alternative<int>(base))
        return "int";
    if (std::holds_alternative<double>(base))
        return "double";
    if (std::holds_alternative<float>(base))
        return "float";
    if (std::holds_alternative<bool>(base))
        return "bool";
    if (std::holds_alternative<std::string>(base))
        return "string";
    if (std::holds_alternative<std::size_t>(base))
        return "size_t";
    if (std::holds_alternative<std::vector<double>>(base))
        return "array<double>";
    if (std::holds_alternative<BlackboardList>(base))
        return "array";
    if (std::holds_alternative<BlackboardMap>(base))
        return "struct";
    if (auto const* boxed = std::get_if<std::any>(&base))
        return boxed->has_value() ? boxed->type().name() : "null";

    return "unknown";
}

// ----------------------------------------------------------------------------
//! \brief Parse \p p_text into \p p_value, keeping the type it already holds.
//! \return \c true when the value was replaced.
// ----------------------------------------------------------------------------
bool assignScalarFromText(BlackboardValue& p_value, std::string const& p_text)
{
    auto const& base = p_value.asBase();

    try
    {
        if (std::holds_alternative<int>(base))
        {
            p_value = std::stoi(p_text);
            return true;
        }
        if (std::holds_alternative<std::size_t>(base))
        {
            p_value = static_cast<std::size_t>(std::stoull(p_text));
            return true;
        }
        if (std::holds_alternative<double>(base))
        {
            p_value = std::stod(p_text);
            return true;
        }
        if (std::holds_alternative<float>(base))
        {
            p_value = std::stof(p_text);
            return true;
        }
    }
    catch (...)
    {
        return false;
    }

    if (std::holds_alternative<bool>(base))
    {
        p_value = (p_text == "true" || p_text == "1");
        return true;
    }
    if (std::holds_alternative<std::string>(base))
    {
        p_value = p_text;
        return true;
    }

    return false;
}

//! \brief Names of the blackboard value types offered by the panel combos.
constexpr std::array<char const*, 5> c_value_type_names = {
    "string", "int", "double", "bool", "struct"};

//! \brief Index of the "struct" entry in \c c_value_type_names.
constexpr int c_struct_type_index = 4;

// ----------------------------------------------------------------------------
//! \brief Build a blackboard value from the text typed by the user.
//! \param[in] p_type_index Index into \c c_value_type_names.
//! \param[in] p_text The typed text, ignored for structs.
// ----------------------------------------------------------------------------
BlackboardValue makeValueFromText(int const p_type_index,
                                  std::string const& p_text)
{
    switch (p_type_index)
    {
        case 0:
            return p_text;
        case 1:
            try
            {
                return std::stoi(p_text);
            }
            catch (...)
            {
                return 0;
            }
        case 2:
            try
            {
                return std::stod(p_text);
            }
            catch (...)
            {
                return 0.0;
            }
        case 3:
            return (p_text == "true" || p_text == "1");
        default:
            return BlackboardMap{};
    }
}

// ----------------------------------------------------------------------------
//! \brief Mutable state the recursive blackboard widgets need to share.
// ----------------------------------------------------------------------------
struct BlackboardDrawContext
{
    //! \brief Text being typed for each entry, keyed by its dotted path.
    std::unordered_map<std::string, std::string>& edit_buffers;
    //! \brief Set when the user asks to add a field to a struct.
    bool& add_field_open;
    //! \brief Dotted path of the struct receiving the new field.
    std::string& add_field_parent_path;
    //! \brief Set when any value was edited during this frame.
    bool modified = false;
};

void drawBlackboardEntry(std::string const& p_key,
                         BlackboardValue& p_value,
                         std::string const& p_full_path,
                         BlackboardDrawContext& p_context);

// ----------------------------------------------------------------------------
//! \brief Draw an editable scalar value.
// ----------------------------------------------------------------------------
void drawScalarEntry(std::string const& p_key,
                     BlackboardValue& p_value,
                     std::string const& p_full_path,
                     BlackboardDrawContext& p_context)
{
    std::string display_value = scalarToEditableText(p_value);
    std::string type_str = valueTypeName(p_value);

    ImGui::TextColored(
        ImVec4(0.6f, 0.6f, 0.8f, 1.0f), "[%s]", type_str.c_str());
    ImGui::SameLine();

    ImGui::Text("%s:", p_key.c_str());
    ImGui::SameLine();

    auto& buffer = p_context.edit_buffers;
    if (buffer.find(p_full_path) == buffer.end())
    {
        buffer[p_full_path] = display_value;
    }

    ImGui::PushItemWidth(100);
    std::string input_id = "##" + p_full_path;
    if (ImGui::InputText(input_id.c_str(),
                         &buffer[p_full_path],
                         ImGuiInputTextFlags_EnterReturnsTrue) &&
        assignScalarFromText(p_value, buffer[p_full_path]))
    {
        p_context.modified = true;
    }
    ImGui::PopItemWidth();
}

// ----------------------------------------------------------------------------
//! \brief Draw a struct (map) entry as a collapsible tree node.
// ----------------------------------------------------------------------------
void drawStructEntry(std::string const& p_key,
                     BlackboardValue& p_value,
                     std::string const& p_full_path,
                     BlackboardDrawContext& p_context)
{
    auto& map = std::get<BlackboardMap>(p_value.asBase());

    ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.8f, 1.0f), "[struct]");
    ImGui::SameLine();

    std::string node_label =
        p_key + " (" + std::to_string(map.size()) + " fields)";
    std::string tree_id = "##tree_" + p_full_path;

    bool node_open = ImGui::TreeNode((node_label + tree_id).c_str());

    ImGui::SameLine();
    std::string add_btn_id = "+##add_" + p_full_path;
    if (ImGui::SmallButton(add_btn_id.c_str()))
    {
        p_context.add_field_open = true;
        p_context.add_field_parent_path = p_full_path;
    }

    if (node_open)
    {
        for (auto& [field_key, field_value] : map)
        {
            ImGui::PushID(field_key.c_str());
            std::string child_path = p_full_path + "." + field_key;
            drawBlackboardEntry(field_key, field_value, child_path, p_context);
            ImGui::PopID();
        }

        ImGui::TreePop();
    }
}

// ----------------------------------------------------------------------------
//! \brief Draw an array entry as a collapsible tree node.
// ----------------------------------------------------------------------------
void drawArrayEntry(std::string const& p_key,
                    BlackboardValue& p_value,
                    std::string const& p_full_path,
                    BlackboardDrawContext& p_context)
{
    if (auto* doubles = std::get_if<std::vector<double>>(&p_value.asBase()))
    {
        auto& vec = *doubles;

        ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "[array<double>]");
        ImGui::SameLine();

        std::string node_label =
            p_key + " [" + std::to_string(vec.size()) + " items]";
        std::string tree_id = "##tree_" + p_full_path;

        if (ImGui::TreeNode((node_label + tree_id).c_str()))
        {
            for (size_t i = 0; i < vec.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                std::string idx_label = "[" + std::to_string(i) + "]:";
                ImGui::Text("%s", idx_label.c_str());
                ImGui::SameLine();

                std::string item_path =
                    p_full_path + "[" + std::to_string(i) + "]";
                auto& buffer = p_context.edit_buffers;
                if (buffer.find(item_path) == buffer.end())
                {
                    buffer[item_path] = std::to_string(vec[i]);
                }

                ImGui::PushItemWidth(80);
                std::string input_id = "##" + item_path;
                if (ImGui::InputText(input_id.c_str(),
                                     &buffer[item_path],
                                     ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    try
                    {
                        vec[i] = std::stod(buffer[item_path]);
                        p_context.modified = true;
                    }
                    catch (...)
                    {
                    }
                }
                ImGui::PopItemWidth();
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
    else if (auto* items = std::get_if<BlackboardList>(&p_value.asBase()))
    {
        auto& vec = *items;

        ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "[array]");
        ImGui::SameLine();

        std::string node_label =
            p_key + " [" + std::to_string(vec.size()) + " items]";
        std::string tree_id = "##tree_" + p_full_path;

        if (ImGui::TreeNode((node_label + tree_id).c_str()))
        {
            for (size_t i = 0; i < vec.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                std::string idx_key = "[" + std::to_string(i) + "]";
                std::string item_path =
                    p_full_path + "[" + std::to_string(i) + "]";
                drawBlackboardEntry(idx_key, vec[i], item_path, p_context);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
}

// ----------------------------------------------------------------------------
//! \brief Draw any blackboard entry, dispatching on its runtime type.
// ----------------------------------------------------------------------------
void drawBlackboardEntry(std::string const& p_key,
                         BlackboardValue& p_value,
                         std::string const& p_full_path,
                         BlackboardDrawContext& p_context)
{
    auto const& base = p_value.asBase();

    if (std::holds_alternative<std::monostate>(base))
    {
        ImGui::TextColored(
            ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s: <null>", p_key.c_str());
        return;
    }

    if (std::holds_alternative<BlackboardMap>(base))
    {
        drawStructEntry(p_key, p_value, p_full_path, p_context);
    }
    else if (std::holds_alternative<BlackboardList>(base) ||
             std::holds_alternative<std::vector<double>>(base))
    {
        drawArrayEntry(p_key, p_value, p_full_path, p_context);
    }
    else if (std::holds_alternative<std::any>(base))
    {
        // Host-owned C++ type: the editor knows nothing about its layout, so it
        // is shown read-only with the type name the compiler gave it.
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f),
                           "[%s]",
                           valueTypeName(p_value).c_str());
        ImGui::SameLine();
        ImGui::Text("%s: <custom>", p_key.c_str());
    }
    else
    {
        drawScalarEntry(p_key, p_value, p_full_path, p_context);
    }
}

// ----------------------------------------------------------------------------
//! \brief Split a dotted blackboard path into its components.
// ----------------------------------------------------------------------------
std::vector<std::string> splitPath(std::string const& p_path)
{
    std::vector<std::string> parts;
    size_t start = 0;
    size_t dot_pos;
    while ((dot_pos = p_path.find('.', start)) != std::string::npos)
    {
        parts.push_back(p_path.substr(start, dot_pos - start));
        start = dot_pos + 1;
    }
    parts.push_back(p_path.substr(start));
    return parts;
}

} // namespace

// ============================================================================
// Frame composition
// ============================================================================

// ----------------------------------------------------------------------------
void Editor::draw(char const* p_title)
{
    handleKeyboardShortcuts();
    drawEditorWindow(p_title);
    drawBlackboardPanel();
}

// ----------------------------------------------------------------------------
void Editor::drawEditorWindow(char const* p_title)
{
    ImGui::Begin(p_title != nullptr ? p_title : "Behavior Tree",
                 nullptr,
                 ImGuiWindowFlags_None);

    switch (m_mode)
    {
        case Mode::Creation:
            showEditorTabs();
            showAddNodePalette();
            break;
        case Mode::Visualizer:
            showVisualizerPanel();
            drawBehaviorTree();
            break;
    }

    ImGui::End();
}

// ----------------------------------------------------------------------------
void Editor::drawMenuBar()
{
    if (ImGui::BeginMenu("File"))
    {
        // The editor cannot browse the file system on its own: the host owns
        // the file dialog and calls back loadFromYaml/saveToYaml.
        if (ImGui::MenuItem("Load Behavior Tree", "Ctrl+O"))
        {
            onFileDialogRequested.emit(FileDialog::Load);
        }

        if (ImGui::MenuItem(
                "Save As...", "Ctrl+S", false, m_mode == Mode::Creation))
        {
            onFileDialogRequested.emit(FileDialog::Save);
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Quit", "Ctrl+Q"))
        {
            onQuitRequested.emit();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        // Auto-layout the nodes
        if (ImGui::MenuItem(
                "Auto Layout", "Ctrl+L", false, m_mode == Mode::Creation))
        {
            autoLayoutNodes();
        }

        ImGui::Separator();

        // Define the layout direction to display the tree (left to right)
        if (ImGui::MenuItem("Layout: Left to Right",
                            nullptr,
                            getCurrentTreeView().layout_direction ==
                                LayoutDirection::LeftToRight,
                            m_mode == Mode::Creation))
        {
            getCurrentTreeView().layout_direction =
                LayoutDirection::LeftToRight;
            autoLayoutNodes();
        }

        // Define the layout direction to display the tree (top to bottom)
        if (ImGui::MenuItem("Layout: Top to Bottom",
                            nullptr,
                            getCurrentTreeView().layout_direction ==
                                LayoutDirection::TopToBottom,
                            m_mode == Mode::Creation))
        {
            getCurrentTreeView().layout_direction =
                LayoutDirection::TopToBottom;
            autoLayoutNodes();
        }

        ImGui::Separator();

        // Add a new node
        if (ImGui::MenuItem(
                "Add Node", "Space", false, m_mode == Mode::Creation))
        {
            m_show_palettes.node_creation = true;
            m_show_palettes.position = ImGui::GetMousePos();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        // Toggle blackboard panel visibility
        if (ImGui::MenuItem("Blackboard", nullptr, m_show_blackboard_panel))
        {
            m_show_blackboard_panel = !m_show_blackboard_panel;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Mode"))
    {
        // Set the editor mode
        if (ImGui::MenuItem("Editor", nullptr, m_mode == Mode::Creation))
        {
            setMode(Mode::Creation);
        }

        // Set the real-time visualizer mode
        if (ImGui::MenuItem("Visualizer", nullptr, m_mode == Mode::Visualizer))
        {
            setMode(Mode::Visualizer);
        }
        ImGui::EndMenu();
    }
}

// ----------------------------------------------------------------------------
void Editor::handleKeyboardShortcuts()
{
    // Skip shortcuts while the user is busy in a popup, including the host's
    // own file dialogs.
    if (isAnyPopupOpen())
        return;

    bool ctrl_pressed = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ||
                        ImGui::IsKeyDown(ImGuiKey_RightCtrl);

    if (!ctrl_pressed)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_O))
    {
        onFileDialogRequested.emit(FileDialog::Load);
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_S) && m_mode == Mode::Creation)
    {
        onFileDialogRequested.emit(FileDialog::Save);
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_L) && m_mode == Mode::Creation)
    {
        autoLayoutNodes();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Q))
    {
        onQuitRequested.emit();
        return;
    }
}

// ============================================================================
// Tree canvas
// ============================================================================

// ----------------------------------------------------------------------------
void Editor::showVisualizerPanel()
{
#if defined(OAKULAR_HAS_SERVER)
    if (!m_server)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                           "No visualizer server attached.");
        return;
    }

    if (!m_server->isConnected())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                           "Waiting for client connection...");
    }
    else if (!m_server->hasReceivedTree())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                           "Client connected. Waiting for tree data...");
    }
    else
    {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                           "Connected - Visualizing tree (%zu nodes)",
                           m_dfs_node_order.size());
    }
#else
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                       "Built without visualizer server support.");
#endif
}

// ----------------------------------------------------------------------------
void Editor::showEditorTabs()
{
    // If only one tree view, skip the tab bar and draw directly
    if (m_tree_views.size() <= 1)
    {
        drawBehaviorTree();
        handleEditModeInteractions();
        return;
    }

    if (!ImGui::BeginTabBar("TreeTabs"))
        return;

    // First pass: draw main trees (non-subtrees)
    for (auto& [name, view] : m_tree_views)
    {
        if (!view.is_subtree)
        {
            drawTreeTab(name, view);
        }
    }

    // Second pass: draw subtrees
    for (auto& [name, view] : m_tree_views)
    {
        if (view.is_subtree)
        {
            drawTreeTab(name, view);
        }
    }

    // Reset the flag after processing
    m_request_tab_change = false;

    ImGui::EndTabBar();
}

// ----------------------------------------------------------------------------
void Editor::drawTreeTab(std::string const& p_name,
                         [[maybe_unused]] TreeView& p_view)
{
    // Use SetSelected only when programmatically changing tab (once)
    ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
    if (m_request_tab_change && m_active_tree_name == p_name)
    {
        flags = ImGuiTabItemFlags_SetSelected;
    }

    // Add unsaved document indicator if modified
    if (m_is_modified)
    {
        flags |= ImGuiTabItemFlags_UnsavedDocument;
    }

    if (ImGui::BeginTabItem(p_name.c_str(), nullptr, flags))
    {
        // Clear selection when switching tabs
        if (m_active_tree_name != p_name)
        {
            m_selected_node_id = -1;
            m_active_tree_name = p_name;
        }
        drawBehaviorTree();
        handleEditModeInteractions();
        ImGui::EndTabItem();
    }
}

// ----------------------------------------------------------------------------
void Editor::showAddNodePalette()
{
    // Open the popup when requested
    if (m_show_palettes.node_creation)
    {
        ImGui::SetNextWindowPos(m_show_palettes.position);
        ImGui::OpenPopup("AddNodePopup");
        m_show_palettes.node_creation = false;
    }

    // Use BeginPopup instead of Begin for auto-closing behavior
    if (!ImGui::BeginPopup("AddNodePopup",
                           ImGuiWindowFlags_NoResize |
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    ImGui::Text("Add Node");
    ImGui::Separator();

    // Group the entries by category, keeping the registration order so that
    // node types added by the host appear after the built-in ones.
    std::string current_category;
    for (NodeType const& node_type : m_node_types)
    {
        if (node_type.category != current_category)
        {
            if (!current_category.empty())
            {
                ImGui::Separator();
            }
            current_category = node_type.category;
            ImGui::TextDisabled("%s", current_category.c_str());
        }

        if (ImGui::Selectable(node_type.name.c_str()))
        {
            addNodeAndLink(node_type.name, node_type.name);
        }
    }

    ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
void Editor::handleEditModeInteractions()
{
    // Skip if not in edit mode
    if (m_mode != Mode::Creation)
        return;

    if (!m_renderer)
        return;

    // Skip if a modal popup is open, but keep drawing the editor popups.
    if (isAnyPopupOpen())
    {
        showNodeContextMenu();
        showNodeEditPopup();
        return;
    }

    // Process renderer events
    ID from_node, to_node;
    if (m_renderer->getLinkCreated(from_node, to_node))
    {
        createLink(from_node, to_node);
    }

    ID link_from, link_to;
    if (m_renderer->getLinkDeleted(link_from, link_to))
    {
        deleteLink(link_from, link_to);
    }

    ID toggled_node_id;
    if (m_renderer->getSubTreeToggled(toggled_node_id))
    {
        toggleSubTreeExpansion(toggled_node_id);
    }

    ID link_void_from;
    if (m_renderer->getLinkDroppedInVoid(link_void_from))
    {
        m_show_palettes.node_creation = true;
        m_show_palettes.position = ImGui::GetMousePos();
        m_show_palettes.canvas_position =
            m_renderer->convertScreenToCanvas(m_show_palettes.position);
        m_pending_link_from_node = link_void_from;
    }

    // Get hovered node once per frame
    ID hovered_id = m_renderer->getHoveredNodeId();

    // Double-click on node: SubTree -> open tab, other -> edit
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hovered_id >= 0)
    {
        m_selected_node_id = hovered_id;
        Node* node = findNode(hovered_id);
        if (node && node->type == "SubTree" && !node->subtree_reference.empty())
        {
            // Find and switch to subtree tab
            auto it = m_tree_views.find(node->subtree_reference);
            if (it != m_tree_views.end())
            {
                m_active_tree_name = node->subtree_reference;
                m_request_tab_change = true;
                return;
            }
        }
        else if (node)
        {
            m_show_palettes.node_edition = true;
        }
    }
    // Single click: select node
    else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_selected_node_id = hovered_id;
    }

    // Right-click: context menu on node, or add palette on empty space
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        if (hovered_id >= 0)
        {
            m_selected_node_id = hovered_id;
            ImGui::OpenPopup("NodeContextMenu");
        }
        else
        {
            m_show_palettes.node_creation = true;
            m_show_palettes.position = ImGui::GetMousePos();
            m_pending_link_from_node = -1;
        }
    }

    showNodeContextMenu();
    showNodeEditPopup();

    // Keyboard shortcuts acting on the selection
    if (m_selected_node_id >= 0 && ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        deleteNode(m_selected_node_id);
        m_selected_node_id = -1;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Space))
    {
        m_show_palettes.node_creation = true;
        m_show_palettes.position = ImGui::GetMousePos();
    }
}

// ----------------------------------------------------------------------------
void Editor::showNodeContextMenu()
{
    if (!ImGui::BeginPopup("NodeContextMenu"))
        return;

    Node* selected = findNode(m_selected_node_id);

    // If node no longer exists, close menu
    if (!selected)
    {
        ImGui::EndPopup();
        return;
    }

    // Go to definition for SubTree nodes
    if (selected->type == "SubTree" && !selected->subtree_reference.empty())
    {
        if (ImGui::MenuItem("Go to Definition"))
        {
            auto it = m_tree_views.find(selected->subtree_reference);
            if (it != m_tree_views.end())
            {
                m_active_tree_name = selected->subtree_reference;
                m_request_tab_change = true;
            }
        }
        ImGui::Separator();
    }

    if (ImGui::MenuItem("Edit", "Double-click"))
    {
        m_show_palettes.node_edition = true;
        ImGui::CloseCurrentPopup();
    }
    if (ImGui::MenuItem("Delete", "Del"))
    {
        deleteNode(m_selected_node_id);
        m_selected_node_id = -1;
        ImGui::CloseCurrentPopup();
    }
    if (ImGui::MenuItem("Set as Root"))
    {
        getCurrentTreeView().root_id = m_selected_node_id;
        // Mark as modified since we changed the tree structure
        m_is_modified = true;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
void Editor::showNodeEditPopup()
{
    Node* node = findNode(m_selected_node_id);
    if (!node)
    {
        m_show_palettes.node_edition = false;
        return;
    }

    // Open the popup when requested
    if (m_show_palettes.node_edition)
    {
        m_node_edit.node = *node;
        m_node_edit.initialized = true;
        m_node_edit.new_input.clear();
        m_node_edit.new_output.clear();

        // Set window position and size before opening popup
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(
            center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);

        ImGui::OpenPopup("Edit Node");
        m_show_palettes.node_edition = false;
    }

    // Use BeginPopupModal for a truly blocking modal popup
    if (ImGui::BeginPopupModal("Edit Node",
                               nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoMove))
    {
        if (!m_node_edit.initialized)
        {
            m_node_edit.node = *node;
            m_node_edit.initialized = true;
        }

        Node& edited = m_node_edit.node;

        // Node Type selection, fed by the registered node types so that the
        // domain nodes of the host can be picked too.
        ImGui::Text("Node Type:");
        ImGui::SameLine();

        std::vector<char const*> type_names;
        type_names.reserve(m_node_types.size());
        int current_type = 0;
        for (size_t i = 0; i < m_node_types.size(); ++i)
        {
            type_names.push_back(m_node_types[i].name.c_str());
            if (edited.type == m_node_types[i].name)
            {
                current_type = static_cast<int>(i);
            }
        }

        if (!type_names.empty() &&
            ImGui::Combo("##Type",
                         &current_type,
                         type_names.data(),
                         static_cast<int>(type_names.size())))
        {
            edited.type = m_node_types[static_cast<size_t>(current_type)].name;
        }

        ImGui::Spacing();

        // Node Name
        ImGui::Text("Name:");
        ImGui::SameLine();
        ImGui::InputText("##Name", &edited.name);

        // SubTree reference (only for SubTree nodes)
        if (edited.type == "SubTree")
        {
            ImGui::Spacing();
            ImGui::Text("Reference:");
            ImGui::SameLine();
            ImGui::InputText("##SubTreeRef", &edited.subtree_reference);

            // Show available SubTrees
            if (ImGui::BeginCombo("##AvailableSubTrees",
                                  edited.subtree_reference.c_str()))
            {
                for (auto const& [name, view] : m_tree_views)
                {
                    if (view.is_subtree)
                    {
                        bool is_selected = (edited.subtree_reference == name);
                        if (ImGui::Selectable(name.c_str(), is_selected))
                        {
                            edited.subtree_reference = name;
                        }
                        if (is_selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Only show inputs/outputs for nodes that can have blackboard ports
        if (canHaveBlackboardPorts(edited.type))
        {
            // Inputs section
            ImGui::Text("Blackboard Inputs:");

            for (size_t i = 0; i < edited.inputs.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                ImGui::BulletText("%s", edited.inputs[i].c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("X"))
                {
                    edited.inputs.erase(edited.inputs.begin() +
                                        static_cast<std::ptrdiff_t>(i));
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }

            ImGui::InputText("##NewInput", &m_node_edit.new_input);
            ImGui::SameLine();
            if (ImGui::Button("Add Input") && !m_node_edit.new_input.empty())
            {
                edited.inputs.push_back(m_node_edit.new_input);
                m_node_edit.new_input.clear();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Outputs section
            ImGui::Text("Blackboard Outputs:");

            for (size_t i = 0; i < edited.outputs.size(); ++i)
            {
                ImGui::PushID(1000 + static_cast<int>(i));
                ImGui::BulletText("%s", edited.outputs[i].c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("X"))
                {
                    edited.outputs.erase(edited.outputs.begin() +
                                         static_cast<std::ptrdiff_t>(i));
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }

            ImGui::InputText("##NewOutput", &m_node_edit.new_output);
            ImGui::SameLine();
            if (ImGui::Button("Add Output") && !m_node_edit.new_output.empty())
            {
                edited.outputs.push_back(m_node_edit.new_output);
                m_node_edit.new_output.clear();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }
        else
        {
            // Clear any existing inputs/outputs for nodes that can't have them
            edited.inputs.clear();
            edited.outputs.clear();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                               "(This node type cannot have blackboard ports)");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        // Validation info
        bool is_decorator =
            (edited.type == "Inverter" || edited.type == "Repeater");
        if (is_decorator && node->children.size() > 1)
        {
            ImGui::TextColored(
                ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                "Warning: Decorators should have exactly 1 child");
        }
        bool is_leaf =
            (edited.type == "Action" || edited.type == "Condition" ||
             edited.type == "Success" || edited.type == "Failure");
        if (is_leaf && !node->children.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                               "Warning: Leaf nodes cannot have children");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Buttons: Cancel and Apply
        float button_width = 120.0f;
        float spacing = 10.0f;
        float total_width = button_width * 2 + spacing;
        float start_x = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;

        if (start_x > 0)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + start_x);

        if (ImGui::Button("Cancel", ImVec2(button_width, 0)))
        {
            // Don't apply changes, just close
            ImGui::CloseCurrentPopup();
            m_node_edit.initialized = false;
            m_node_edit.new_input.clear();
            m_node_edit.new_output.clear();
        }

        ImGui::SameLine(0, spacing);

        if (ImGui::Button("Apply", ImVec2(button_width, 0)))
        {
            node->type = edited.type;
            node->name = edited.name;
            node->subtree_reference = edited.subtree_reference;
            node->inputs = edited.inputs;
            node->outputs = edited.outputs;

            m_is_modified = true;
            onNodeModified.emit(m_selected_node_id);
            ImGui::CloseCurrentPopup();
            m_node_edit.initialized = false;
            m_node_edit.new_input.clear();
            m_node_edit.new_output.clear();
        }

        ImGui::EndPopup();
    }

    // If popup was closed (e.g., via Escape key), reset temp state
    if (!ImGui::IsPopupOpen("Edit Node") && m_node_edit.initialized)
    {
        m_node_edit.initialized = false;
        m_node_edit.new_input.clear();
        m_node_edit.new_output.clear();
    }
}

// ============================================================================
// Blackboard panel
// ============================================================================

// ----------------------------------------------------------------------------
void Editor::drawBlackboardPanel()
{
    if (!m_show_blackboard_panel || !m_blackboard)
        return;

    ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Blackboard", &m_show_blackboard_panel))
    {
        ImGui::End();
        return;
    }

    auto& state = m_blackboard_panel;

    ImGui::Text("Add Variable:");
    ImGui::PushItemWidth(100);
    ImGui::InputText("Name##NewVar", &state.new_var_name);
    ImGui::SameLine();

    // Only show value input for non-struct types
    if (state.new_var_type != c_struct_type_index)
    {
        ImGui::InputText("Value##NewVar", &state.new_var_value);
    }
    else
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(empty struct)");
    }
    ImGui::PopItemWidth();

    ImGui::PushItemWidth(80);
    ImGui::Combo("Type##NewVar",
                 &state.new_var_type,
                 c_value_type_names.data(),
                 static_cast<int>(c_value_type_names.size()));
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Add") && !state.new_var_name.empty())
    {
        m_blackboard->set(
            state.new_var_name,
            makeValueFromText(state.new_var_type, state.new_var_value));

        state.new_var_name.clear();
        state.new_var_value.clear();
        m_is_modified = true;
    }

    ImGui::Separator();
    ImGui::Text("Variables:");

    // Display existing variables using the recursive helpers
    std::vector<std::string> keys_to_remove;
    BlackboardDrawContext context{state.edit_buffers,
                                  state.add_field_open,
                                  state.add_field_parent_path,
                                  false};

    for (std::string const& key : m_blackboard->keys())
    {
        auto raw_value = m_blackboard->raw(key);

        if (!raw_value.has_value())
            continue;

        ImGui::PushID(key.c_str());

        // Make a mutable copy for editing
        BlackboardValue value_copy = *raw_value;

        context.modified = false;
        drawBlackboardEntry(key, value_copy, key, context);

        if (context.modified)
        {
            m_blackboard->set(key, value_copy);
            m_is_modified = true;
        }

        // Delete button for top-level entries
        ImGui::SameLine();
        if (ImGui::SmallButton("X"))
        {
            keys_to_remove.push_back(key);
        }

        ImGui::PopID();
    }

    // Remove marked keys
    for (auto const& key : keys_to_remove)
    {
        m_blackboard->remove(key);
        m_is_modified = true;
    }

    // Handle add field popup for structs
    if (state.add_field_open)
    {
        ImGui::OpenPopup("Add Field##AddFieldPopup");
        state.add_field_open = false;
        state.add_field_name.clear();
        state.add_field_value.clear();
        state.add_field_type = 0;
    }

    if (ImGui::BeginPopupModal("Add Field##AddFieldPopup",
                               nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Add field to: %s", state.add_field_parent_path.c_str());
        ImGui::Separator();

        ImGui::InputText("Field Name", &state.add_field_name);

        ImGui::Combo("Type",
                     &state.add_field_type,
                     c_value_type_names.data(),
                     static_cast<int>(c_value_type_names.size()));

        if (state.add_field_type != c_struct_type_index)
        {
            ImGui::InputText("Value", &state.add_field_value);
        }

        ImGui::Separator();

        if (ImGui::Button("Cancel", ImVec2(80, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Add", ImVec2(80, 0)) &&
            !state.add_field_name.empty())
        {
            std::vector<std::string> path_parts =
                splitPath(state.add_field_parent_path);
            std::string const& root_key = path_parts[0];

            auto root_value = m_blackboard->raw(root_key);
            if (root_value.has_value())
            {
                BlackboardValue value_copy = *root_value;

                // Navigate down to the target struct
                BlackboardValue* target = &value_copy;
                for (size_t i = 1; i < path_parts.size(); ++i)
                {
                    BlackboardMap* map = asStruct(target);
                    if (map == nullptr)
                    {
                        target = nullptr;
                        break;
                    }
                    auto it = map->find(path_parts[i]);
                    target = (it != map->end()) ? &it->second : nullptr;
                }

                if (BlackboardMap* map = asStruct(target))
                {
                    (*map)[state.add_field_name] = makeValueFromText(
                        state.add_field_type, state.add_field_value);

                    m_blackboard->set(root_key, value_copy);
                    m_is_modified = true;
                }
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace oakular
