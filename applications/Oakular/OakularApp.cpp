/**
 * @file OakularApp.cpp
 * @brief Oakular standalone application - UI widgets implementation
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

// Must be defined before including imgui.h (through OakularApp.hpp)
#define IMGUI_DEFINE_MATH_OPERATORS

#include "OakularApp.hpp"
#include "Renderer.hpp"

#include <imgui_stdlib.h>
#include <yaml-cpp/yaml.h>

#include <iostream>

// ----------------------------------------------------------------------------
static const std::array<const char*, 10> c_node_types = //
    {"Action",
     "Condition",
     "Failure",
     "Inverter",
     "Parallel",
     "Repeater",
     "Selector",
     "Sequence",
     "SubTree",
     "Success"};

// ----------------------------------------------------------------------------
//! \brief Check if a node type can have blackboard input/output ports.
//! Only Action, Condition, and SubTree nodes can have blackboard ports.
//! \param[in] p_type The node type to check.
//! \return true if the node type can have blackboard ports.
// ----------------------------------------------------------------------------
static bool canHaveBlackboardPorts(const std::string& p_type)
{
    return p_type == "Action" || p_type == "Condition" || p_type == "SubTree";
}

// ----------------------------------------------------------------------------
OakularApp::OakularApp(size_t const p_width, size_t const p_height)
    : IDE(p_width, p_height)
{
}

// ----------------------------------------------------------------------------
void OakularApp::onDrawMainPanel()
{
    // Handle keyboard shortcuts
    handleKeyboardShortcuts();

    ImGui::Begin("Behavior Tree", nullptr, ImGuiWindowFlags_None);

    // Handle different editor modes
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
        default:
            std::cout << "Unknown editor mode" << std::endl;
            break;
    }

    ImGui::End();

    showBlackboardPanel();
    showFileDialogs();
    showQuitConfirmationPopup();
}

// ----------------------------------------------------------------------------
void OakularApp::handleKeyboardShortcuts()
{
    // Skip shortcuts if a modal popup is open
    if (ImGui::IsPopupOpen("Edit Node") || ImGui::IsPopupOpen("Confirm Quit") ||
        ImGui::IsPopupOpen("LoadYamlDlgKey") ||
        ImGui::IsPopupOpen("SaveYamlDlgKey"))
    {
        return;
    }

    bool ctrl_pressed = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ||
                        ImGui::IsKeyDown(ImGuiKey_RightCtrl);

    if (!ctrl_pressed)
        return;

    // Handle Ctrl+O: Load Behavior Tree
    if (ImGui::IsKeyPressed(ImGuiKey_O))
    {
        IGFD::FileDialogConfig config;
        config.path = ".";
        config.countSelectionMax = 1;
        config.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog(
            "LoadYamlDlgKey", "Choose YAML File", ".yaml,.yml", config);
        return;
    }

    // Handle Ctrl+S: Save As... (only in Creation mode)
    if (ImGui::IsKeyPressed(ImGuiKey_S) && m_mode == Mode::Creation)
    {
        IGFD::FileDialogConfig config;
        config.path = ".";
        config.countSelectionMax = 1;
        config.flags =
            ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ConfirmOverwrite;
        ImGuiFileDialog::Instance()->OpenDialog(
            "SaveYamlDlgKey", "Save YAML File", ".yaml", config);
        return;
    }

    // Handle Ctrl+L: Auto Layout (only in Creation mode)
    if (ImGui::IsKeyPressed(ImGuiKey_L) && m_mode == Mode::Creation)
    {
        autoLayoutNodes();
        return;
    }

    // Handle Ctrl+Q: Quit (works globally, not just in menu)
    if (ImGui::IsKeyPressed(ImGuiKey_Q))
    {
        if (m_is_modified)
        {
            m_show_palettes.quit_confirmation = true;
        }
        else
        {
            halt();
        }
        return;
    }
}

// ----------------------------------------------------------------------------
void OakularApp::showVisualizerPanel()
{
    if (!m_server)
        return;

    m_server->update();

    if (!m_server->isConnected())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                           "Waiting for client connection...");
        // Reset tree loaded flag when disconnected
        m_dfs_node_order.clear();
    }
    else if (!m_server->hasReceivedTree())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                           "Client connected. Waiting for tree data...");
    }
    else
    {
        // Tree has been received - load it if not already done
        if (m_dfs_node_order.empty())
        {
            loadFromYamlString(m_server->getYamlData());
            std::cout << "Loaded tree with " << m_dfs_node_order.size()
                      << " nodes in DFS order" << std::endl;
        }

        // Update node states if we have new data
        if (m_server->hasStateUpdate())
        {
            // Update runtime_status for each node by its ID
            for (auto& [node_id, node] : m_nodes)
            {
                node.runtime_status = m_server->getNodeState(node_id);
            }
            m_server->clearStateUpdate();
        }

        // Show connection status
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                           "Connected - Visualizing tree (%zu nodes)",
                           m_dfs_node_order.size());
    }
}

// ----------------------------------------------------------------------------
void OakularApp::showEditorTabs()
{
    // If only one tree view, skip the tab bar and draw directly
    if (m_tree_views.size() <= 1)
    {
        drawBehaviorTree();
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
void OakularApp::drawTreeTab(const std::string& name,
                             [[maybe_unused]] TreeView& view)
{
    // Use SetSelected only when programmatically changing tab (once)
    ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
    if (m_request_tab_change && m_active_tree_name == name)
    {
        flags = ImGuiTabItemFlags_SetSelected;
    }

    // Add unsaved document indicator if modified
    if (m_is_modified)
    {
        flags |= ImGuiTabItemFlags_UnsavedDocument;
    }

    if (ImGui::BeginTabItem(name.c_str(), nullptr, flags))
    {
        // Clear selection when switching tabs
        if (m_active_tree_name != name)
        {
            m_selected_node_id = -1;
            m_active_tree_name = name;
        }
        drawBehaviorTree();
        handleEditModeInteractions();
        ImGui::EndTabItem();
    }
}

// ----------------------------------------------------------------------------
void OakularApp::showAddNodePalette()
{
    // Open the popup when requested
    if (m_show_palettes.node_creation)
    {
        ImGui::SetNextWindowPos(m_show_palettes.position);
        ImGui::OpenPopup("AddNodePopup");
        m_show_palettes.node_creation = false; // Stay open
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

    // Show the node creation palette
    for (const auto& node_type : c_node_types)
    {
        if (node_type && ImGui::Selectable(node_type))
        {
            addNodeAndLink(node_type, node_type);
        }
    }

    ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
void OakularApp::handleEditModeInteractions()
{
    // Skip if not in edit mode
    if (m_mode != Mode::Creation)
        return;

    // Skip if modal popup is open
    if (ImGui::IsPopupOpen("Edit Node") || ImGui::IsPopupOpen("Confirm Quit"))
    {
        // Still show the popups but don't process other interactions
        showNodeContextMenu();
        showNodeEditPopup();
        return;
    }

    // Process renderer events
    int from_node, to_node;
    if (m_renderer->getLinkCreated(from_node, to_node))
    {
        createLink(from_node, to_node);
    }

    int link_from, link_to;
    if (m_renderer->getLinkDeleted(link_from, link_to))
    {
        deleteLink(link_from, link_to);
    }

    int toggled_node_id;
    if (m_renderer->getSubTreeToggled(toggled_node_id))
    {
        toggleSubTreeExpansion(toggled_node_id);
    }

    int link_void_from;
    if (m_renderer->getLinkDroppedInVoid(link_void_from))
    {
        m_show_palettes.node_creation = true;
        m_show_palettes.position = ImGui::GetMousePos();
        m_show_palettes.canvas_position =
            m_renderer->convertScreenToCanvas(m_show_palettes.position);
        m_pending_link_from_node = link_void_from;
    }

    // Get hovered node once per frame
    int hovered_id = m_renderer->getHoveredNodeId();

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
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
        !ImGui::IsPopupOpen("NodeContextMenu") &&
        !ImGui::IsPopupOpen("AddNodePopup"))
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

    // Keyboard shortcuts
    if (!ImGui::IsPopupOpen("NodeContextMenu") &&
        !ImGui::IsPopupOpen("AddNodePopup"))
    {
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
}

// ----------------------------------------------------------------------------
void OakularApp::showNodeContextMenu()
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
void OakularApp::showNodeEditPopup()
{
    Node* node = findNode(m_selected_node_id);
    if (!node)
    {
        m_show_palettes.node_edition = false;
        return;
    }

    // Static variables to store temporary editing state
    static Node temp_node;
    static bool temp_node_initialized = false;
    static std::string temp_new_input;
    static std::string temp_new_output;

    // Open the popup when requested
    if (m_show_palettes.node_edition)
    {
        // Initialize temporary node with current node state
        temp_node = *node;
        temp_node_initialized = true;
        temp_new_input.clear();
        temp_new_output.clear();

        // Set window position and size before opening popup
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(
            center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);

        ImGui::OpenPopup("Edit Node");
        // Reset the request flag
        m_show_palettes.node_edition = false;
    }

    // Use BeginPopupModal for a truly blocking modal popup
    if (ImGui::BeginPopupModal("Edit Node",
                               nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoMove))
    {
        if (!temp_node_initialized)
        {
            temp_node = *node;
            temp_node_initialized = true;
        }

        // Node Type selection
        ImGui::Text("Node Type:");
        ImGui::SameLine();

        static int current_type = 0;
        // Find current type index
        for (size_t i = 0; i < c_node_types.size(); i++)
        {
            if (temp_node.type == c_node_types[i])
            {
                current_type = static_cast<int>(i);
                break;
            }
        }

        if (ImGui::Combo("##Type",
                         &current_type,
                         c_node_types.data(),
                         static_cast<int>(c_node_types.size())))
        {
            temp_node.type = c_node_types[static_cast<size_t>(current_type)];
        }

        ImGui::Spacing();

        // Node Name
        ImGui::Text("Name:");
        ImGui::SameLine();
        ImGui::InputText("##Name", &temp_node.name);

        // SubTree reference (only for SubTree nodes)
        if (temp_node.type == "SubTree")
        {
            ImGui::Spacing();
            ImGui::Text("Reference:");
            ImGui::SameLine();
            ImGui::InputText("##SubTreeRef", &temp_node.subtree_reference);

            // Show available SubTrees
            if (ImGui::BeginCombo("##AvailableSubTrees",
                                  temp_node.subtree_reference.c_str()))
            {
                for (const auto& [name, view] : m_tree_views)
                {
                    if (view.is_subtree)
                    {
                        bool is_selected =
                            (temp_node.subtree_reference == name);
                        if (ImGui::Selectable(name.c_str(), is_selected))
                        {
                            temp_node.subtree_reference = name;
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
        if (canHaveBlackboardPorts(temp_node.type))
        {
            // Inputs section
            ImGui::Text("Blackboard Inputs:");

            for (size_t i = 0; i < temp_node.inputs.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                ImGui::BulletText("%s", temp_node.inputs[i].c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("X"))
                {
                    temp_node.inputs.erase(temp_node.inputs.begin() +
                                           static_cast<std::ptrdiff_t>(i));
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }

            ImGui::InputText("##NewInput", &temp_new_input);
            ImGui::SameLine();
            if (ImGui::Button("Add Input") && !temp_new_input.empty())
            {
                temp_node.inputs.push_back(temp_new_input);
                temp_new_input.clear();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Outputs section
            ImGui::Text("Blackboard Outputs:");

            for (size_t i = 0; i < temp_node.outputs.size(); ++i)
            {
                ImGui::PushID(1000 + static_cast<int>(i));
                ImGui::BulletText("%s", temp_node.outputs[i].c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("X"))
                {
                    temp_node.outputs.erase(temp_node.outputs.begin() +
                                            static_cast<std::ptrdiff_t>(i));
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }

            ImGui::InputText("##NewOutput", &temp_new_output);
            ImGui::SameLine();
            if (ImGui::Button("Add Output") && !temp_new_output.empty())
            {
                temp_node.outputs.push_back(temp_new_output);
                temp_new_output.clear();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }
        else
        {
            // Clear any existing inputs/outputs for nodes that can't have them
            temp_node.inputs.clear();
            temp_node.outputs.clear();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                               "(This node type cannot have blackboard ports)");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        // Validation info (use temp_node for validation)
        bool is_decorator =
            (temp_node.type == "Inverter" || temp_node.type == "Repeater");
        if (is_decorator && node->children.size() > 1)
        {
            ImGui::TextColored(
                ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                "Warning: Decorators should have exactly 1 child");
        }
        bool is_leaf =
            (temp_node.type == "Action" || temp_node.type == "Condition" ||
             temp_node.type == "Success" || temp_node.type == "Failure");
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
            temp_node_initialized = false;
            temp_new_input.clear();
            temp_new_output.clear();
        }

        ImGui::SameLine(0, spacing);

        if (ImGui::Button("Apply", ImVec2(button_width, 0)))
        {
            // Apply changes from temp_node to actual node
            node->type = temp_node.type;
            node->name = temp_node.name;
            node->subtree_reference = temp_node.subtree_reference;
            node->inputs = temp_node.inputs;
            node->outputs = temp_node.outputs;

            m_is_modified = true;
            // Emit modification signal
            onNodeModified.emit(m_selected_node_id);
            ImGui::CloseCurrentPopup();
            temp_node_initialized = false;
            temp_new_input.clear();
            temp_new_output.clear();
        }

        ImGui::EndPopup();
    }

    // If popup was closed (e.g., via Escape key), reset temp state
    if (!ImGui::IsPopupOpen("Edit Node") && temp_node_initialized)
    {
        temp_node_initialized = false;
        temp_new_input.clear();
        temp_new_output.clear();
    }
}

// ----------------------------------------------------------------------------
// Helper function to convert std::any to a displayable string
static std::string anyToDisplayString(const std::any& value)
{
    if (!value.has_value())
        return "<empty>";

    try
    {
        if (value.type() == typeid(int))
            return std::to_string(std::any_cast<int>(value));
        if (value.type() == typeid(double))
            return std::to_string(std::any_cast<double>(value));
        if (value.type() == typeid(bool))
            return std::any_cast<bool>(value) ? "true" : "false";
        if (value.type() == typeid(std::string))
            return std::any_cast<std::string>(value);
        if (value.type() == typeid(std::vector<double>))
        {
            auto& vec = std::any_cast<const std::vector<double>&>(value);
            std::string result = "[";
            for (size_t i = 0; i < vec.size(); ++i)
            {
                if (i > 0)
                    result += ", ";
                result += std::to_string(vec[i]);
            }
            result += "]";
            return result;
        }
        if (value.type() == typeid(std::unordered_map<std::string, std::any>))
        {
            return "{...}"; // Complex map, just show indicator
        }
        if (value.type() == typeid(std::vector<std::any>))
        {
            return "[...]"; // Complex array, just show indicator
        }
    }
    catch (...)
    {
        return "<error>";
    }

    return "<unknown>";
}

// Helper function to get the type name of a std::any value
static std::string anyTypeName(const std::any& value)
{
    if (!value.has_value())
        return "null";

    if (value.type() == typeid(int))
        return "int";
    if (value.type() == typeid(double))
        return "double";
    if (value.type() == typeid(bool))
        return "bool";
    if (value.type() == typeid(std::string))
        return "string";
    if (value.type() == typeid(std::vector<double>))
        return "array<double>";
    if (value.type() == typeid(std::unordered_map<std::string, std::any>))
        return "struct";
    if (value.type() == typeid(std::vector<std::any>))
        return "array";

    return "unknown";
}

// Forward declarations for recursive blackboard entry functions
static bool drawBlackboardEntry(const std::string& key,
                                std::any& value,
                                bt::Blackboard* blackboard,
                                const std::string& full_path,
                                std::vector<std::string>& keys_to_remove,
                                bool& modified);

// Draw a scalar value (editable)
static bool drawScalarEntry(const std::string& key,
                            std::any& value,
                            const std::string& full_path,
                            bool& modified)
{
    static std::unordered_map<std::string, std::string> edit_buffers;

    std::string display_value = anyToDisplayString(value);
    std::string type_str = anyTypeName(value);

    // Type indicator
    ImGui::TextColored(
        ImVec4(0.6f, 0.6f, 0.8f, 1.0f), "[%s]", type_str.c_str());
    ImGui::SameLine();

    // Key name
    ImGui::Text("%s:", key.c_str());
    ImGui::SameLine();

    // Initialize edit buffer if needed
    if (edit_buffers.find(full_path) == edit_buffers.end())
    {
        edit_buffers[full_path] = display_value;
    }

    ImGui::PushItemWidth(100);
    std::string input_id = "##" + full_path;
    if (ImGui::InputText(input_id.c_str(),
                         &edit_buffers[full_path],
                         ImGuiInputTextFlags_EnterReturnsTrue))
    {
        // Update the value based on type
        if (value.type() == typeid(int))
        {
            try
            {
                value = std::stoi(edit_buffers[full_path]);
                modified = true;
            }
            catch (...)
            {
            }
        }
        else if (value.type() == typeid(double))
        {
            try
            {
                value = std::stod(edit_buffers[full_path]);
                modified = true;
            }
            catch (...)
            {
            }
        }
        else if (value.type() == typeid(bool))
        {
            value = (edit_buffers[full_path] == "true" ||
                     edit_buffers[full_path] == "1");
            modified = true;
        }
        else if (value.type() == typeid(std::string))
        {
            value = edit_buffers[full_path];
            modified = true;
        }
    }
    ImGui::PopItemWidth();

    return modified;
}

// Popup state for adding fields to structs
struct AddFieldPopup
{
    bool open = false;
    std::string parent_path;
    char field_name[128] = "";
    char field_value[256] = "";
    int field_type = 0; // 0=string, 1=int, 2=double, 3=bool, 4=struct
};
static AddFieldPopup s_add_field_popup;

// Draw a struct (map) entry with TreeNode
static bool drawStructEntry(const std::string& key,
                            std::any& value,
                            bt::Blackboard* blackboard,
                            const std::string& full_path,
                            std::vector<std::string>& keys_to_remove,
                            bool& modified)
{
    auto& map =
        *std::any_cast<std::unordered_map<std::string, std::any>>(&value);

    // Count fields for display
    size_t field_count = map.size();

    ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.8f, 1.0f), "[struct]");
    ImGui::SameLine();

    std::string node_label =
        key + " (" + std::to_string(field_count) + " fields)";
    std::string tree_id = "##tree_" + full_path;

    bool node_open = ImGui::TreeNode((node_label + tree_id).c_str());

    // Add field button (always visible, next to tree node)
    ImGui::SameLine();
    std::string add_btn_id = "+##add_" + full_path;
    if (ImGui::SmallButton(add_btn_id.c_str()))
    {
        s_add_field_popup.open = true;
        s_add_field_popup.parent_path = full_path;
        s_add_field_popup.field_name[0] = '\0';
        s_add_field_popup.field_value[0] = '\0';
        s_add_field_popup.field_type = 0;
    }

    if (node_open)
    {
        // Draw each field recursively
        for (auto& [field_key, field_value] : map)
        {
            ImGui::PushID(field_key.c_str());
            std::string child_path = full_path + "." + field_key;
            drawBlackboardEntry(field_key,
                                field_value,
                                blackboard,
                                child_path,
                                keys_to_remove,
                                modified);
            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    return modified;
}

// Draw an array entry with TreeNode
static bool drawArrayEntry(const std::string& key,
                           std::any& value,
                           bt::Blackboard* blackboard,
                           const std::string& full_path,
                           std::vector<std::string>& keys_to_remove,
                           bool& modified)
{
    // Check if it's a simple double array or generic array
    if (value.type() == typeid(std::vector<double>))
    {
        auto& vec = *std::any_cast<std::vector<double>>(&value);

        ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "[array<double>]");
        ImGui::SameLine();

        std::string node_label =
            key + " [" + std::to_string(vec.size()) + " items]";
        std::string tree_id = "##tree_" + full_path;

        if (ImGui::TreeNode((node_label + tree_id).c_str()))
        {
            for (size_t i = 0; i < vec.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                std::string idx_label = "[" + std::to_string(i) + "]:";
                ImGui::Text("%s", idx_label.c_str());
                ImGui::SameLine();

                static std::unordered_map<std::string, std::string>
                    array_edit_buffers;
                std::string item_path =
                    full_path + "[" + std::to_string(i) + "]";

                if (array_edit_buffers.find(item_path) ==
                    array_edit_buffers.end())
                {
                    array_edit_buffers[item_path] = std::to_string(vec[i]);
                }

                ImGui::PushItemWidth(80);
                std::string input_id = "##" + item_path;
                if (ImGui::InputText(input_id.c_str(),
                                     &array_edit_buffers[item_path],
                                     ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    try
                    {
                        vec[i] = std::stod(array_edit_buffers[item_path]);
                        modified = true;
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
    else if (value.type() == typeid(std::vector<std::any>))
    {
        auto& vec = *std::any_cast<std::vector<std::any>>(&value);

        ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "[array]");
        ImGui::SameLine();

        std::string node_label =
            key + " [" + std::to_string(vec.size()) + " items]";
        std::string tree_id = "##tree_" + full_path;

        if (ImGui::TreeNode((node_label + tree_id).c_str()))
        {
            for (size_t i = 0; i < vec.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                std::string idx_key = "[" + std::to_string(i) + "]";
                std::string item_path =
                    full_path + "[" + std::to_string(i) + "]";
                drawBlackboardEntry(idx_key,
                                    vec[i],
                                    blackboard,
                                    item_path,
                                    keys_to_remove,
                                    modified);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }

    return modified;
}

// Main recursive function to draw a blackboard entry
static bool drawBlackboardEntry(const std::string& key,
                                std::any& value,
                                bt::Blackboard* blackboard,
                                const std::string& full_path,
                                std::vector<std::string>& keys_to_remove,
                                bool& modified)
{
    if (!value.has_value())
    {
        ImGui::TextColored(
            ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s: <null>", key.c_str());
        return false;
    }

    // Check type and dispatch to appropriate handler
    if (value.type() == typeid(std::unordered_map<std::string, std::any>))
    {
        return drawStructEntry(
            key, value, blackboard, full_path, keys_to_remove, modified);
    }
    else if (value.type() == typeid(std::vector<std::any>) ||
             value.type() == typeid(std::vector<double>))
    {
        return drawArrayEntry(
            key, value, blackboard, full_path, keys_to_remove, modified);
    }
    else
    {
        // Scalar value
        return drawScalarEntry(key, value, full_path, modified);
    }
}

// ----------------------------------------------------------------------------
void OakularApp::showBlackboardPanel()
{
    if (!m_show_blackboard_panel || !m_blackboard)
        return;

    ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Blackboard", &m_show_blackboard_panel))
    {
        ImGui::End();
        return;
    }

    // Add new variable section
    static char new_var_name[128] = "";
    static char new_var_value[256] = "";
    static int new_var_type = 0; // 0=string, 1=int, 2=double, 3=bool, 4=struct

    ImGui::Text("Add Variable:");
    ImGui::PushItemWidth(100);
    ImGui::InputText("Name##NewVar", new_var_name, sizeof(new_var_name));
    ImGui::SameLine();

    // Only show value input for non-struct types
    if (new_var_type != 4)
    {
        ImGui::InputText("Value##NewVar", new_var_value, sizeof(new_var_value));
    }
    else
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(empty struct)");
    }
    ImGui::PopItemWidth();

    ImGui::PushItemWidth(80);
    const char* type_names[] = {"string", "int", "double", "bool", "struct"};
    ImGui::Combo("Type##NewVar", &new_var_type, type_names, 5);
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Add") && strlen(new_var_name) > 0)
    {
        std::string name(new_var_name);
        std::string val(new_var_value);

        switch (new_var_type)
        {
            case 0: // string
                m_blackboard->set(name, val);
                break;
            case 1: // int
                try
                {
                    m_blackboard->set(name, std::stoi(val));
                }
                catch (...)
                {
                    m_blackboard->set(name, 0);
                }
                break;
            case 2: // double
                try
                {
                    m_blackboard->set(name, std::stod(val));
                }
                catch (...)
                {
                    m_blackboard->set(name, 0.0);
                }
                break;
            case 3: // bool
                m_blackboard->set(name, val == "true" || val == "1");
                break;
            case 4: // struct (empty map)
                m_blackboard->set(name,
                                  std::unordered_map<std::string, std::any>{});
                break;
        }

        new_var_name[0] = '\0';
        new_var_value[0] = '\0';
        m_is_modified = true;
    }

    ImGui::Separator();
    ImGui::Text("Variables:");

    // Display existing variables using recursive function
    YAML::Node bb_yaml = bt::BlackboardSerializer::dump(*m_blackboard);

    std::vector<std::string> keys_to_remove;
    bool modified = false;

    for (auto yaml_it = bb_yaml.begin(); yaml_it != bb_yaml.end(); ++yaml_it)
    {
        std::string key = yaml_it->first.as<std::string>();
        auto raw_value = m_blackboard->raw(key);

        if (!raw_value.has_value())
            continue;

        ImGui::PushID(key.c_str());

        // Make a mutable copy for editing
        std::any value_copy = *raw_value;

        // Draw the entry recursively
        drawBlackboardEntry(
            key, value_copy, m_blackboard.get(), key, keys_to_remove, modified);

        // If modified, update the blackboard
        if (modified)
        {
            m_blackboard->set(key, value_copy);
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
    for (const auto& key : keys_to_remove)
    {
        m_blackboard->remove(key);
        m_is_modified = true;
    }

    // Handle add field popup for structs
    if (s_add_field_popup.open)
    {
        ImGui::OpenPopup("Add Field##AddFieldPopup");
        s_add_field_popup.open = false;
    }

    if (ImGui::BeginPopupModal("Add Field##AddFieldPopup",
                               nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Add field to: %s", s_add_field_popup.parent_path.c_str());
        ImGui::Separator();

        ImGui::InputText("Field Name",
                         s_add_field_popup.field_name,
                         sizeof(s_add_field_popup.field_name));

        const char* field_types[] = {
            "string", "int", "double", "bool", "struct"};
        ImGui::Combo("Type", &s_add_field_popup.field_type, field_types, 5);

        if (s_add_field_popup.field_type != 4)
        {
            ImGui::InputText("Value",
                             s_add_field_popup.field_value,
                             sizeof(s_add_field_popup.field_value));
        }

        ImGui::Separator();

        if (ImGui::Button("Cancel", ImVec2(80, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Add", ImVec2(80, 0)) &&
            strlen(s_add_field_popup.field_name) > 0)
        {
            // Parse the parent path to find the struct
            std::string parent_path = s_add_field_popup.parent_path;
            std::string root_key;
            std::vector<std::string> path_parts;

            // Split path by '.'
            size_t start = 0;
            size_t dot_pos;
            while ((dot_pos = parent_path.find('.', start)) !=
                   std::string::npos)
            {
                path_parts.push_back(
                    parent_path.substr(start, dot_pos - start));
                start = dot_pos + 1;
            }
            path_parts.push_back(parent_path.substr(start));

            root_key = path_parts[0];

            // Get the root value
            auto root_value = m_blackboard->raw(root_key);
            if (root_value.has_value())
            {
                std::any value_copy = *root_value;

                // Navigate to the target struct
                std::any* target = &value_copy;
                for (size_t i = 1; i < path_parts.size() && target; ++i)
                {
                    if (target->type() ==
                        typeid(std::unordered_map<std::string, std::any>))
                    {
                        auto& map = *std::any_cast<
                            std::unordered_map<std::string, std::any>>(target);
                        auto it = map.find(path_parts[i]);
                        if (it != map.end())
                        {
                            target = &it->second;
                        }
                        else
                        {
                            target = nullptr;
                        }
                    }
                    else
                    {
                        target = nullptr;
                    }
                }

                // Add the new field
                if (target &&
                    target->type() ==
                        typeid(std::unordered_map<std::string, std::any>))
                {
                    auto& map = *std::any_cast<
                        std::unordered_map<std::string, std::any>>(target);

                    std::string field_name(s_add_field_popup.field_name);
                    std::string field_val(s_add_field_popup.field_value);

                    switch (s_add_field_popup.field_type)
                    {
                        case 0: // string
                            map[field_name] = field_val;
                            break;
                        case 1: // int
                            try
                            {
                                map[field_name] = std::stoi(field_val);
                            }
                            catch (...)
                            {
                                map[field_name] = 0;
                            }
                            break;
                        case 2: // double
                            try
                            {
                                map[field_name] = std::stod(field_val);
                            }
                            catch (...)
                            {
                                map[field_name] = 0.0;
                            }
                            break;
                        case 3: // bool
                            map[field_name] =
                                (field_val == "true" || field_val == "1");
                            break;
                        case 4: // struct
                            map[field_name] =
                                std::unordered_map<std::string, std::any>{};
                            break;
                    }

                    // Update the blackboard
                    m_blackboard->set(root_key, value_copy);
                    m_is_modified = true;
                }
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (modified)
    {
        m_is_modified = true;
    }

    ImGui::End();
}

// ----------------------------------------------------------------------------
void OakularApp::showFileDialogs()
{
    // Load dialog
    if (ImGuiFileDialog::Instance()->Display("LoadYamlDlgKey"))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            std::string filePathName =
                ImGuiFileDialog::Instance()->GetFilePathName();
            loadFromYaml(filePathName);
        }

        ImGuiFileDialog::Instance()->Close();
    }

    // Save dialog
    if (ImGuiFileDialog::Instance()->Display("SaveYamlDlgKey"))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            std::string filePathName =
                ImGuiFileDialog::Instance()->GetFilePathName();
            saveToYaml(filePathName);
        }

        ImGuiFileDialog::Instance()->Close();
    }
}

// ----------------------------------------------------------------------------
void OakularApp::showQuitConfirmationPopup()
{
    // Open the popup when requested
    if (m_show_palettes.quit_confirmation)
    {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(
            center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Confirm Quit");
        m_show_palettes.quit_confirmation = false;
    }

    // Quit confirmation popup
    if (ImGui::BeginPopupModal(
            "Confirm Quit", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("You have unsaved changes. Are you sure you want to quit?");
        ImGui::Separator();

        // Open save dialog
        if (ImGui::Button("Save and Quit", ImVec2(120, 0)))
        {
            IGFD::FileDialogConfig config;
            config.path = ".";
            config.countSelectionMax = 1;
            config.flags = ImGuiFileDialogFlags_Modal |
                           ImGuiFileDialogFlags_ConfirmOverwrite;
            ImGuiFileDialog::Instance()->OpenDialog(
                "SaveYamlDlgKey", "Save YAML File", ".yaml", config);
            ImGui::CloseCurrentPopup();
        }

        // Quit without saving
        ImGui::SameLine();
        if (ImGui::Button("Quit Without Saving", ImVec2(160, 0)))
        {
            halt();
            ImGui::CloseCurrentPopup();
        }

        // Cancel
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
