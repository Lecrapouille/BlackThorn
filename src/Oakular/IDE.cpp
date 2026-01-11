/**
 * @file IDE.cpp
 * @brief Oakular - Behavior tree editor implementation
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "IDE.hpp"
#include "Renderer.hpp"

#include <ImGuiFileDialog.h>
#include <imgui_stdlib.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
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
//! \brief Extract the filename without extension from a file path.
//! \param[in] filepath The full file path.
//! \return The filename without extension, or empty string if extraction fails.
// ----------------------------------------------------------------------------
static std::string extractFileNameWithoutExtension(std::string const& filepath)
{
    if (filepath.empty())
        return "";

    try
    {
        std::filesystem::path path(filepath);
        std::string filename = path.stem().string();
        return filename.empty() ? "" : filename;
    }
    catch (...)
    {
        // Fallback: manual extraction if filesystem fails
        size_t last_slash = filepath.find_last_of("/\\");
        size_t start = (last_slash == std::string::npos) ? 0 : last_slash + 1;
        size_t last_dot = filepath.find_last_of('.');
        size_t end = (last_dot == std::string::npos || last_dot < start)
                         ? filepath.length()
                         : last_dot;
        return filepath.substr(start, end - start);
    }
}

// ----------------------------------------------------------------------------
IDE::IDE(size_t const p_width, size_t const p_height)
    : Application(p_width, p_height)
{
}

// ----------------------------------------------------------------------------
bool IDE::onSetup()
{
    setTitle("Oakular - BlackThorn Editor");
    m_renderer = std::make_unique<Renderer>();
    m_server = std::make_unique<Server>();
    reset();
    return true;
}

// ----------------------------------------------------------------------------
void IDE::reset()
{
    m_nodes.clear();
    m_tree_views.clear();
    m_unique_node_id = 1;
    m_selected_node_id = -1;
    m_active_tree_name.clear();
    m_behavior_tree_filepath.clear();
    m_is_modified = false;
    m_show_palettes.node_creation = false;
    m_show_palettes.node_edition = false;
    m_show_palettes.quit_confirmation = false;
    m_show_palettes.position = ImVec2(0, 0);
    m_show_palettes.canvas_position = ImVec2(0, 0);
    m_blackboard = std::make_shared<bt::Blackboard>();
}

// ----------------------------------------------------------------------------
IDE::~IDE()
{
    onTeardown();
}

// ----------------------------------------------------------------------------
void IDE::onTeardown()
{
    if (m_server)
    {
        m_server->stop();
        m_server.reset();
    }

    if (m_renderer)
    {
        m_renderer->shutdown();
        m_renderer.reset();
    }
}

// ----------------------------------------------------------------------------
void IDE::onDrawMenuBar()
{
    if (ImGui::BeginMenu("File"))
    {
        // Open file dialog to load a YAML file
        if (ImGui::MenuItem("Load Behavior Tree", "Ctrl+O"))
        {
            IGFD::FileDialogConfig config;
            config.path = ".";
            config.countSelectionMax = 1;
            config.flags = ImGuiFileDialogFlags_Modal;
            ImGuiFileDialog::Instance()->OpenDialog(
                "LoadYamlDlgKey", "Choose YAML File", ".yaml,.yml", config);
        }

        // Open file dialog to save a YAML file
        if (ImGui::MenuItem(
                "Save As...", "Ctrl+S", false, m_mode == Mode::Creation))
        {
            IGFD::FileDialogConfig config;
            config.path = ".";
            config.countSelectionMax = 1;
            config.flags = ImGuiFileDialogFlags_Modal |
                           ImGuiFileDialogFlags_ConfirmOverwrite;
            ImGuiFileDialog::Instance()->OpenDialog(
                "SaveYamlDlgKey", "Save YAML File", ".yaml", config);
        }
        ImGui::Separator();

        // Quit the application
        if (ImGui::MenuItem("Quit", "Ctrl+Q"))
        {
            if (m_is_modified)
            {
                m_show_palettes.quit_confirmation = true;
            }
            else
            {
                halt();
            }
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
void IDE::onDrawMainPanel()
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
void IDE::setMode(Mode const p_mode)
{
    if (m_mode == p_mode)
        return;

    m_mode = p_mode;
    switch (m_mode)
    {
        case Mode::Visualizer:
            // Clear the current tree when entering visualizer mode
            reset();
            if (m_server && !m_server->isConnected())
            {
                m_server->start();
            }
            break;
        case Mode::Creation:
            if (m_server)
            {
                m_server->stop();
            }
            break;
        default:
            std::cout << "Unknown editor mode" << std::endl;
            break;
    }
}

// ----------------------------------------------------------------------------
void IDE::handleKeyboardShortcuts()
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
void IDE::showVisualizerPanel()
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
void IDE::showEditorTabs()
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
void IDE::drawTreeTab(const std::string& name, [[maybe_unused]] TreeView& view)
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
        ImGui::EndTabItem();
    }
}

// ----------------------------------------------------------------------------
void IDE::showAddNodePalette()
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
void IDE::drawBehaviorTree()
{
    if (!m_renderer)
        return;

    // Get current view (non-const because we'll modify node_positions)
    TreeView& current_view = getCurrentTreeView();

    // Collect visible nodes from current root
    std::unordered_set<int> visible_node_ids;
    int current_root_id = current_view.root_id;

    // Debug: ensure we have a valid root_id
    if (current_root_id < 0)
    {
        // If no root_id, try to find one or use the first node
        // This shouldn't happen, but handle it gracefully
        return;
    }

    collectVisibleNodes(current_root_id, visible_node_ids);

    // Only include orphan nodes when viewing the main tree, not subtrees
    // This allows new unconnected nodes to be visible in the main tree
    if (!current_view.is_subtree)
    {
        // Build set of subtree root IDs (these should not appear in main tree)
        std::unordered_set<int> subtree_root_ids;
        for (const auto& [name, view] : m_tree_views)
        {
            if (view.is_subtree && view.root_id >= 0)
            {
                subtree_root_ids.insert(view.root_id);
            }
        }

        // Include orphan nodes (nodes without parents that aren't the root)
        // BUT exclude subtree definition roots
        for (const auto& [id, node] : m_nodes)
        {
            if (node.parent == -1 && id != current_root_id &&
                subtree_root_ids.find(id) == subtree_root_ids.end())
            {
                visible_node_ids.insert(id);
                // Also collect their children if any
                collectVisibleNodes(id, visible_node_ids);
            }
        }
    }

    // Filter nodes to only visible ones and sync positions from current view
    std::unordered_map<int, Node> visible_nodes;

    // Sync positions from current view's node_positions to nodes (for Renderer)
    // Also update m_nodes so positions are in sync
    for (const auto& [id, node] : m_nodes)
    {
        if (visible_node_ids.count(id) > 0)
        {
            visible_nodes[id] = node;
            // Copy position from current view's node_positions
            auto pos_it = current_view.node_positions.find(id);
            if (pos_it != current_view.node_positions.end())
            {
                visible_nodes[id].position = pos_it->second;
                // Also update the node in m_nodes to keep them in sync
                m_nodes[id].position = pos_it->second;
            }
            else
            {
                // No position stored, use default
                visible_nodes[id].position = ImVec2(0, 0);
                m_nodes[id].position = ImVec2(0, 0);
            }
        }
    }

    // Generate links from parent-child relationships
    std::vector<Link> visible_links;
    int link_id = 0;
    for (const auto& [id, node] : visible_nodes)
    {
        for (int child_id : node.children)
        {
            if (visible_node_ids.count(child_id) > 0)
            {
                visible_links.push_back({link_id++, id, child_id});
            }
        }
    }

    // Render the graph
    bool const is_edit_mode = (m_mode == Mode::Creation);
    IDE::LayoutDirection layout_dir = getCurrentTreeView().layout_direction;
    int layout_dir_int = static_cast<int>(layout_dir);
    m_renderer->drawBehaviorTree(visible_nodes,
                                 visible_links,
                                 layout_dir_int,
                                 m_blackboard.get(),
                                 !is_edit_mode);

    // Save positions back to current view's node_positions (in case Renderer
    // modified them)
    for (auto& [id, node] : visible_nodes)
    {
        current_view.node_positions[id] = node.position;
    }

    // Handle interactions (edit mode only)
    if (is_edit_mode)
    {
        handleEditModeInteractions();
    }
}

// ----------------------------------------------------------------------------
void IDE::collectVisibleNodes(int root_id,
                              std::unordered_set<int>& visible_nodes)
{
    Node* node = findNode(root_id);
    if (!node)
        return;

    // Add this node to visible set
    visible_nodes.insert(root_id);

    // Process children
    for (int child_id : node->children)
    {
        Node* child = findNode(child_id);
        if (!child)
            continue;

        // Check if this child is from a collapsed SubTree
        if (node->type == "SubTree" && !node->is_expanded)
        {
            // This is a collapsed SubTree, don't include its children
            continue;
        }

        // Recursively collect visible nodes
        collectVisibleNodes(child_id, visible_nodes);
    }
}

// ----------------------------------------------------------------------------
void IDE::handleEditModeInteractions()
{
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
void IDE::showNodeContextMenu()
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
        // Note: The port d'entrée will still be visible since we changed the
        // logic to show input pins for all nodes (including root) for visual
        // consistency
    }
    ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
void IDE::showNodeEditPopup()
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
                current_type = i;
                break;
            }
        }

        if (ImGui::Combo("##Type",
                         &current_type,
                         c_node_types.data(),
                         c_node_types.size()))
        {
            temp_node.type = c_node_types[current_type];
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
                ImGui::PushID(i);
                ImGui::BulletText("%s", temp_node.inputs[i].c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("X"))
                {
                    temp_node.inputs.erase(temp_node.inputs.begin() + i);
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
                ImGui::PushID(1000 + i);
                ImGui::BulletText("%s", temp_node.outputs[i].c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("X"))
                {
                    temp_node.outputs.erase(temp_node.outputs.begin() + i);
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
void IDE::showBlackboardPanel()
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
void IDE::showFileDialogs()
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
void IDE::showQuitConfirmationPopup()
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

// ----------------------------------------------------------------------------
int IDE::addNode(std::string const& p_type, std::string const& p_name)
{
    int id = getNextNodeId();
    IDE::Node node;
    node.id = id;
    node.type = p_type;
    node.name = p_name;

    // Use the palette position if available (stored before palette was shown)
    // Check if we have a pending link to determine if node was created from
    // drag-drop
    m_nodes.emplace(id, std::move(node));

    // Set position in current view's node_positions
    ImVec2 initial_position;
    if (m_pending_link_from_node >= 0)
    {
        // Position at the link drop point (already in canvas coordinates)
        initial_position = m_show_palettes.canvas_position;
    }
    else
    {
        // Normal node creation - use palette position or current mouse position
        if (m_show_palettes.position.x != 0.0f ||
            m_show_palettes.position.y != 0.0f)
        {
            // Convert screen position (from palette window) to canvas
            // coordinates
            initial_position =
                m_renderer->convertScreenToCanvas(m_show_palettes.position);
        }
        else
        {
            // Convert screen mouse position to canvas coordinates
            ImVec2 screen_pos = ImGui::GetMousePos();
            initial_position = m_renderer->convertScreenToCanvas(screen_pos);
        }
    }
    setNodePosition(id, initial_position);

    if (m_nodes.size() == 1u)
    {
        getCurrentTreeView().root_id = id;
    }

    m_is_modified = true;

    std::cout << "Added node: " << p_type << " (" << p_name << ")" << std::endl;

    return id;
}

// ----------------------------------------------------------------------------
void IDE::addNodeAndLink(std::string const& p_type, std::string const& p_name)
{
    int new_node_id = addNode(p_type, p_name);

    // If we have a pending link, complete it now
    if (m_pending_link_from_node >= 0)
    {
        createLink(m_pending_link_from_node, new_node_id);
        m_pending_link_from_node = -1;
    }

    ImGui::CloseCurrentPopup();
}

// ----------------------------------------------------------------------------
void IDE::deleteNode(int const p_node_id)
{
    Node* node = findNode(p_node_id);
    if (node)
    {
        // Remove from parent's children list
        if (node->parent >= 0)
        {
            Node* parent = findNode(node->parent);
            if (parent)
            {
                auto& children = parent->children;
                children.erase(
                    std::remove(children.begin(), children.end(), p_node_id),
                    children.end());
            }
        }

        // Clear parent reference for all children
        for (int child_id : node->children)
        {
            Node* child = findNode(child_id);
            if (child)
            {
                child->parent = -1;
            }
        }
    }

    // Remove the node
    m_nodes.erase(p_node_id);

    // Update the root node if needed
    if (getCurrentTreeView().root_id == p_node_id)
    {
        getCurrentTreeView().root_id = -1;
    }

    m_is_modified = true;
}

// ----------------------------------------------------------------------------
void IDE::createLink(int const p_from_node, int const p_to_node)
{
    Node* from = findNode(p_from_node);
    Node* to = findNode(p_to_node);
    if (!from || !to)
        return;

    // Check if the link already exists
    if (std::find(from->children.begin(), from->children.end(), p_to_node) !=
        from->children.end())
    {
        return;
    }

    // Remove any existing parent of the target node
    // This allows replacing an existing connection by dragging a new link
    if (to->parent >= 0)
    {
        Node* old_parent = findNode(to->parent);
        if (old_parent)
        {
            auto& children = old_parent->children;
            children.erase(
                std::remove(children.begin(), children.end(), p_to_node),
                children.end());
        }
    }

    // Create the parent-child relationship
    from->children.push_back(p_to_node);
    to->parent = p_from_node;

    m_is_modified = true;

    // Emit signal
    onLinkCreated.emit(p_from_node, p_to_node);
}

// ----------------------------------------------------------------------------
void IDE::deleteLink(int const p_from_node, int const p_to_node)
{
    Node* from = findNode(p_from_node);
    Node* to = findNode(p_to_node);

    if (from && to)
    {
        // Remove from parent's children list
        auto& children = from->children;
        auto it = std::find(children.begin(), children.end(), p_to_node);
        if (it != children.end())
        {
            children.erase(it);
            to->parent = -1;

            m_is_modified = true;

            // Emit signal (using a combined ID for backwards compatibility)
            onLinkDeleted.emit(p_from_node * 10000 + p_to_node);
        }
    }
}

// ----------------------------------------------------------------------------
IDE::Node* IDE::findNode(int p_id)
{
    auto it = m_nodes.find(p_id);
    return it != m_nodes.end() ? &it->second : nullptr;
}

// ----------------------------------------------------------------------------
ImVec2 IDE::getNodePosition(ID node_id)
{
    TreeView& current_view = getCurrentTreeView();
    auto it = current_view.node_positions.find(node_id);
    if (it != current_view.node_positions.end())
    {
        return it->second;
    }
    return ImVec2(0, 0); // Default position
}

// ----------------------------------------------------------------------------
void IDE::setNodePosition(ID node_id, ImVec2 position)
{
    TreeView& current_view = getCurrentTreeView();
    current_view.node_positions[node_id] = position;
}

// ----------------------------------------------------------------------------
IDE::TreeView& IDE::getCurrentTreeView()
{
    // Ensure we always have at least one TreeView
    if (m_tree_views.empty())
    {
        // Create a default tree view with the current filepath name or "Main"
        std::string default_name =
            m_behavior_tree_filepath.empty()
                ? "Main"
                : extractFileNameWithoutExtension(m_behavior_tree_filepath);
        m_tree_views[default_name] = {
            default_name, false, -1, LayoutDirection::TopToBottom, {}};
        m_active_tree_name = default_name;
    }

    // Ensure active_tree_name is valid
    auto it = m_tree_views.find(m_active_tree_name);
    if (it == m_tree_views.end())
    {
        // Prefer a non-subtree (main tree) if available
        for (auto& [name, view] : m_tree_views)
        {
            if (!view.is_subtree)
            {
                m_active_tree_name = name;
                return view;
            }
        }
        // Fallback to first available
        m_active_tree_name = m_tree_views.begin()->first;
        it = m_tree_views.begin();
    }

    return it->second;
}

// ----------------------------------------------------------------------------
IDE::TreeView* IDE::findTreeViewByRootId(int root_id)
{
    for (auto& [name, view] : m_tree_views)
    {
        if (view.root_id == root_id)
        {
            return &view;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
void IDE::autoLayoutNodes()
{
    int root_id = getCurrentTreeView().root_id;
    if (root_id < 0)
        return;

    Node* root = findNode(root_id);
    if (!root)
        return;

    float maxExtent = 0;
    layoutNodeRecursive(root, 100.0f, 100.0f, maxExtent);
}

// ----------------------------------------------------------------------------
void IDE::layoutNodeRecursive(IDE::Node* p_node,
                              float p_x,
                              float p_y,
                              float& p_max_extent)
{
    if (!p_node)
        return;

    // Groot-style layout with proper spacing
    constexpr float NODE_HORIZONTAL_SPACING = 40.0f; // Reduced spacing
    constexpr float NODE_VERTICAL_SPACING = 60.0f;   // Reduced spacing
    constexpr float MIN_NODE_WIDTH = 150.0f;
    constexpr float MIN_NODE_HEIGHT = 80.0f;

    // Use minimum dimensions for layout
    ImVec2 node_size(MIN_NODE_WIDTH, MIN_NODE_HEIGHT);

    // Get layout direction from current view
    LayoutDirection layout_dir = getCurrentTreeView().layout_direction;

    // If the node has no children, set its position and return
    if (p_node->children.empty())
    {
        setNodePosition(p_node->id, ImVec2(p_x, p_y));

        if (layout_dir == LayoutDirection::LeftToRight)
        {
            p_max_extent = std::max(p_max_extent, p_y + node_size.y);
        }
        else // TopToBottom
        {
            p_max_extent = std::max(p_max_extent, p_x + node_size.x);
        }
        return;
    }

    // Layout children recursively first (post-order)
    float child_start_pos =
        (layout_dir == LayoutDirection::LeftToRight) ? p_y : p_x;

    std::vector<ImVec2> child_positions;

    for (size_t i = 0; i < p_node->children.size(); ++i)
    {
        Node* child = findNode(p_node->children[i]);
        if (child)
        {
            float child_extent_before = p_max_extent;

            if (layout_dir == LayoutDirection::LeftToRight)
            {
                // Left to right: parent to children goes right (X increases)
                // Siblings spread vertically (Y increases)
                float child_x = p_x + node_size.x + NODE_VERTICAL_SPACING;
                layoutNodeRecursive(
                    child, child_x, child_start_pos, p_max_extent);
                child_positions.push_back(getNodePosition(child->id));

                // Move to next sibling position vertically (use minimum
                // dimensions)
                child_start_pos += std::max(p_max_extent - child_extent_before,
                                            MIN_NODE_HEIGHT) +
                                   NODE_HORIZONTAL_SPACING;
            }
            else // TopToBottom
            {
                // Top to bottom: parent to children goes down (Y increases)
                // Siblings spread horizontally (X increases)
                float child_y = p_y + node_size.y + NODE_VERTICAL_SPACING;
                layoutNodeRecursive(
                    child, child_start_pos, child_y, p_max_extent);
                child_positions.push_back(getNodePosition(child->id));

                // Move to next sibling position horizontally (use minimum
                // dimensions)
                child_start_pos += std::max(p_max_extent - child_extent_before,
                                            MIN_NODE_WIDTH) +
                                   NODE_HORIZONTAL_SPACING;
            }
        }
    }

    // Center parent relative to children
    if (!child_positions.empty())
    {
        if (layout_dir == LayoutDirection::LeftToRight)
        {
            // Center vertically over children (children spread vertically)
            float min_child_y = child_positions[0].y;
            float max_child_y = child_positions[child_positions.size() - 1].y;
            // Add height of last child (use minimum)
            max_child_y += MIN_NODE_HEIGHT;
            float centerY =
                (min_child_y + max_child_y) / 2.0f - node_size.y / 2.0f;
            ImVec2 pos = ImVec2(p_x, std::max(p_y, centerY));
            setNodePosition(p_node->id, pos);
            p_max_extent = std::max(p_max_extent, pos.x + node_size.x);
        }
        else // TopToBottom
        {
            // Center horizontally over children (children spread horizontally)
            float min_child_x = child_positions[0].x;
            float max_child_x = child_positions[child_positions.size() - 1].x;
            // Add width of last child (use minimum)
            max_child_x += MIN_NODE_WIDTH;
            float centerX =
                (min_child_x + max_child_x) / 2.0f - node_size.x / 2.0f;
            ImVec2 pos = ImVec2(std::max(p_x, centerX), p_y);
            setNodePosition(p_node->id, pos);
            p_max_extent = std::max(p_max_extent, pos.y + node_size.y);
        }
    }
    else
    {
        setNodePosition(p_node->id, ImVec2(p_x, p_y));
    }
}

// ----------------------------------------------------------------------------
void IDE::toggleSubTreeExpansion(int node_id)
{
    IDE::Node* node = findNode(node_id);
    if (!node || node->type != "SubTree")
        return;

    bool success = false;
    if (node->is_expanded)
    {
        success = collapseSubTree(node);
    }
    else
    {
        success = expandSubTree(node);
    }

    // Only toggle the flag if the operation succeeded
    if (success)
    {
        node->is_expanded = !node->is_expanded;
    }
}

// ----------------------------------------------------------------------------
bool IDE::expandSubTree(IDE::Node* subtree_node)
{
    if (!subtree_node || subtree_node->subtree_reference.empty())
        return false;

    // Find the SubTree definition
    auto it = m_tree_views.find(subtree_node->subtree_reference);
    if (it == m_tree_views.end() || !it->second.is_subtree)
        return false;

    int subtree_root_id = it->second.root_id;
    Node* subtree_root = findNode(subtree_root_id);
    if (!subtree_root)
        return false;

    // Link the SubTree root as a child of the SubTree node
    if (std::find(subtree_node->children.begin(),
                  subtree_node->children.end(),
                  subtree_root_id) == subtree_node->children.end())
    {
        subtree_node->children.push_back(subtree_root_id);
        subtree_root->parent = subtree_node->id;
    }

    // Relayout after expansion
    autoLayoutNodes();
    return true;
}

// ----------------------------------------------------------------------------
bool IDE::collapseSubTree(IDE::Node* subtree_node)
{
    if (!subtree_node || subtree_node->subtree_reference.empty())
        return false;

    // Find the SubTree definition
    auto it = m_tree_views.find(subtree_node->subtree_reference);
    if (it == m_tree_views.end() || !it->second.is_subtree)
        return false;

    int subtree_root_id = it->second.root_id;

    // Remove the link between SubTree node and its expanded content
    auto child_it = std::find(subtree_node->children.begin(),
                              subtree_node->children.end(),
                              subtree_root_id);
    if (child_it != subtree_node->children.end())
    {
        subtree_node->children.erase(child_it);
    }

    Node* subtree_root = findNode(subtree_root_id);
    if (subtree_root)
    {
        subtree_root->parent = -1;
    }

    // Relayout after collapse
    autoLayoutNodes();
    return true;
}

// ----------------------------------------------------------------------------
void IDE::loadFromYaml(const std::string& p_filepath)
{
    std::cout << "Loading tree from: " << p_filepath << std::endl;

    try
    {
        YAML::Node yaml = YAML::LoadFile(p_filepath);

        // Clear existing data
        m_nodes.clear();
        m_tree_views.clear();
        m_unique_node_id = 1;
        m_selected_node_id = -1;
        m_active_tree_name.clear();
        m_dfs_node_order.clear();

        // Create a fresh blackboard and parse the Blackboard section
        m_blackboard = std::make_shared<bt::Blackboard>();
        if (yaml["Blackboard"])
        {
            bt::BlackboardSerializer::load(*m_blackboard, yaml["Blackboard"]);
            std::cout << "Loaded Blackboard with variables" << std::endl;
        }

        // Parse the BehaviorTree section
        if (yaml["BehaviorTree"])
        {
            YAML::Node tree_node = yaml["BehaviorTree"];
            int root_id = parseYamlNode(tree_node, -1);

            if (root_id < 0)
            {
                std::cerr << "Failed to parse BehaviorTree section"
                          << std::endl;
                return;
            }

            // Extract filename without extension for the tree name
            std::string tree_name = extractFileNameWithoutExtension(p_filepath);

            // Add main tree to views
            m_tree_views[tree_name] = {
                tree_name, false, root_id, LayoutDirection::TopToBottom, {}};
            m_active_tree_name = tree_name;
        }
        else
        {
            std::cerr << "No BehaviorTree section found in YAML" << std::endl;
            return;
        }

        // Parse SubTrees section
        if (yaml["SubTrees"])
        {
            YAML::Node subtrees = yaml["SubTrees"];
            for (auto it = subtrees.begin(); it != subtrees.end(); ++it)
            {
                std::string subtree_name = it->first.as<std::string>();
                YAML::Node subtree_def = it->second;

                // Parse the subtree
                int subtree_root_id = parseYamlNode(subtree_def, -1);

                if (subtree_root_id < 0)
                {
                    std::cerr << "Failed to parse SubTree: " << subtree_name
                              << std::endl;
                    continue;
                }

                // Add subtree to views
                m_tree_views[subtree_name] = {subtree_name,
                                              true,
                                              subtree_root_id,
                                              LayoutDirection::TopToBottom,
                                              {}};

                std::cout << "Loaded SubTree: " << subtree_name << std::endl;
            }
        }

        // Link SubTree nodes to their definitions
        for (auto& [id, node] : m_nodes)
        {
            if (node.type == "SubTree" && !node.subtree_reference.empty())
            {
                // Subtree reference already set during parsing
                std::cout << "SubTree node '" << node.name
                          << "' references: " << node.subtree_reference
                          << std::endl;
            }
        }

        // Auto-layout the nodes for each tree view
        // Save the current active name to restore it later
        std::string saved_active_name = m_active_tree_name;

        for (auto& [name, view] : m_tree_views)
        {
            // Switch to this view temporarily to layout its nodes
            m_active_tree_name = name;
            autoLayoutNodes();

            // Positions are already saved in node_positions by
            // setNodePosition() called in autoLayoutNodes(). We just need to
            // sync node.position for consistency with the Renderer
            std::unordered_set<ID> visible_nodes;
            if (view.root_id >= 0)
            {
                collectVisibleNodes(view.root_id, visible_nodes);
            }
            for (ID id : visible_nodes)
            {
                auto node_it = m_nodes.find(id);
                auto pos_it = view.node_positions.find(id);
                if (node_it != m_nodes.end() &&
                    pos_it != view.node_positions.end())
                {
                    // Sync node.position from node_positions (for Renderer
                    // compatibility)
                    node_it->second.position = pos_it->second;
                }
            }
        }

        // Restore the original active name
        m_active_tree_name = saved_active_name;

        m_is_modified = false;
        m_behavior_tree_filepath = p_filepath;

        // Count subtrees
        size_t subtree_count = 0;
        for (const auto& [name, view] : m_tree_views)
        {
            if (view.is_subtree)
                subtree_count++;
        }

        std::cout << "Tree loaded successfully: " << m_nodes.size()
                  << " nodes, " << subtree_count << " subtrees" << std::endl;
    }
    catch (const YAML::Exception& e)
    {
        std::cerr << "YAML parsing error: " << e.what() << std::endl;
    }
}

// ----------------------------------------------------------------------------
void IDE::loadFromYamlString(const std::string& p_yaml_content)
{
    std::cout << "Loading tree from YAML string" << std::endl;

    try
    {
        YAML::Node yaml = YAML::Load(p_yaml_content);

        // Clear existing data
        m_nodes.clear();
        m_tree_views.clear();
        m_unique_node_id = 1;
        m_selected_node_id = -1;
        m_active_tree_name.clear();
        m_dfs_node_order.clear();

        // Create a fresh blackboard
        m_blackboard = std::make_shared<bt::Blackboard>();
        if (yaml["Blackboard"])
        {
            bt::BlackboardSerializer::load(*m_blackboard, yaml["Blackboard"]);
            std::cout << "Loaded Blackboard with variables" << std::endl;
        }

        // Parse SubTrees section first (so they exist when linking)
        if (yaml["SubTrees"])
        {
            YAML::Node subtrees = yaml["SubTrees"];
            for (auto it = subtrees.begin(); it != subtrees.end(); ++it)
            {
                std::string subtree_name = it->first.as<std::string>();
                YAML::Node subtree_def = it->second;

                // Parse the subtree
                int subtree_root_id = parseYamlNode(subtree_def, -1);

                if (subtree_root_id < 0)
                {
                    std::cerr << "Failed to parse SubTree: " << subtree_name
                              << std::endl;
                    continue;
                }

                // Add subtree to views
                m_tree_views[subtree_name] = {subtree_name,
                                              true,
                                              subtree_root_id,
                                              LayoutDirection::TopToBottom,
                                              {}};

                std::cout << "Loaded SubTree: " << subtree_name << std::endl;
            }
        }

        // Parse the BehaviorTree section
        if (yaml["BehaviorTree"])
        {
            YAML::Node tree_node = yaml["BehaviorTree"];
            int root_id = parseYamlNode(tree_node, -1);

            if (root_id < 0)
            {
                std::cerr << "Failed to parse BehaviorTree section"
                          << std::endl;
                return;
            }

            // Use "Visualizer" as the tree name
            std::string tree_name = "Visualizer";

            // Add main tree to views
            m_tree_views[tree_name] = {
                tree_name, false, root_id, LayoutDirection::TopToBottom, {}};
            m_active_tree_name = tree_name;
        }
        else
        {
            std::cerr << "No BehaviorTree section found in YAML" << std::endl;
            return;
        }

        // Link SubTree nodes to their definitions
        for (auto& [id, node] : m_nodes)
        {
            if (node.type == "SubTree" && !node.subtree_reference.empty())
            {
                std::cout << "SubTree node '" << node.name
                          << "' references: " << node.subtree_reference
                          << std::endl;
            }
        }

        // Auto-layout the nodes
        autoLayoutNodes();

        m_is_modified = false;

        std::cout << "Tree loaded from string: " << m_nodes.size() << " nodes, "
                  << m_dfs_node_order.size() << " in DFS order" << std::endl;
    }
    catch (const YAML::Exception& e)
    {
        std::cerr << "YAML parsing error: " << e.what() << std::endl;
    }
}

void IDE::saveToYaml(const std::string& p_filepath)
{
    std::cout << "Saving tree to: " << p_filepath << std::endl;

    int root_id = getCurrentTreeView().root_id;
    if (root_id < 0)
    {
        std::cerr << "No root node to save" << std::endl;
        return;
    }

    Node* root = findNode(root_id);
    if (!root)
    {
        std::cerr << "Root node not found" << std::endl;
        return;
    }

    std::ofstream file(p_filepath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file for writing: " << p_filepath
                  << std::endl;
        return;
    }

    YAML::Emitter out;

    // Save main BehaviorTree
    out << YAML::BeginMap;

    // Save Blackboard section using the serializer
    if (m_blackboard)
    {
        YAML::Node blackboard_yaml =
            bt::BlackboardSerializer::dump(*m_blackboard);
        if (blackboard_yaml.size() > 0)
        {
            out << YAML::Key << "Blackboard";
            out << YAML::Value << blackboard_yaml;
        }
    }

    out << YAML::Key << "BehaviorTree";
    out << YAML::Value;
    serializeNodeToYaml(out, root);

    // Save SubTrees if any
    bool has_subtrees = false;
    for (const auto& [name, view] : m_tree_views)
    {
        if (view.is_subtree)
        {
            has_subtrees = true;
            break;
        }
    }

    if (has_subtrees)
    {
        out << YAML::Key << "SubTrees";
        out << YAML::Value << YAML::BeginMap;

        for (const auto& [name, view] : m_tree_views)
        {
            if (view.is_subtree && view.root_id >= 0)
            {
                Node* subtree_root = findNode(view.root_id);
                if (subtree_root)
                {
                    out << YAML::Key << name;
                    out << YAML::Value;
                    // Pass true to indicate this is a subtree definition
                    serializeNodeToYaml(out, subtree_root, true);
                }
            }
        }

        out << YAML::EndMap;
    }

    out << YAML::EndMap;

    file << out.c_str();
    file.close();

    m_is_modified = false;
    m_behavior_tree_filepath = p_filepath;

    std::cout << "Tree saved successfully to: " << p_filepath << std::endl;
}

// ============================================================================
// Tree Conversion
// ============================================================================

void IDE::buildTreeFromNodes()
{
    // TODO: Pour une future version si nécessaire
    // Pour l'instant, la sauvegarde utilise directement les Node
}

void IDE::buildNodesFromTree(bt::Node& p_root)
{
    buildNodesFromTreeRecursive(p_root, -1);
    autoLayoutNodes();
}

int IDE::buildNodesFromTreeRecursive(bt::Node& p_node, int p_parent_id)
{
    int node_id = getNextNodeId();
    IDE::Node editor_node;
    editor_node.id = node_id;
    editor_node.type = p_node.type();
    editor_node.name = p_node.name;
    editor_node.parent = p_parent_id;

    // Pour Composite nodes, récupérer les enfants
    if (auto composite = dynamic_cast<bt::Composite*>(&p_node))
    {
        for (auto& child : composite->getChildren())
        {
            int child_id = buildNodesFromTreeRecursive(*child, node_id);
            if (child_id >= 0)
            {
                editor_node.children.push_back(child_id);
            }
        }
    }
    // Pour Decorator nodes
    else if (auto decorator = dynamic_cast<bt::Decorator*>(&p_node))
    {
        if (decorator->hasChild())
        {
            int child_id =
                buildNodesFromTreeRecursive(decorator->getChild(), node_id);
            if (child_id >= 0)
            {
                editor_node.children.push_back(child_id);
            }
        }
    }

    // Utiliser emplace au lieu de operator[] pour éviter le constructeur
    // par défaut
    m_nodes.emplace(node_id, std::move(editor_node));

    if (p_parent_id < 0)
    {
        getCurrentTreeView().root_id = node_id;
    }

    return node_id;
}

void IDE::serializeNodeToYaml(YAML::Emitter& p_out,
                              IDE::Node* p_node,
                              bool is_subtree_definition)
{
    if (!p_node)
        return;

    p_out << YAML::BeginMap;
    p_out << YAML::Key << p_node->type;
    p_out << YAML::Value << YAML::BeginMap;
    p_out << YAML::Key << "name" << YAML::Value << p_node->name;

    // For SubTree nodes, save the reference (only in main tree, not in
    // definitions)
    if (p_node->type == "SubTree" && !p_node->subtree_reference.empty() &&
        !is_subtree_definition)
    {
        p_out << YAML::Key << "reference" << YAML::Value
              << p_node->subtree_reference;
    }

    // Save inputs as parameters with ${variable} reference format
    if (!p_node->inputs.empty())
    {
        p_out << YAML::Key << "inputs" << YAML::Value << YAML::BeginMap;
        for (const auto& input : p_node->inputs)
        {
            p_out << YAML::Key << input << YAML::Value << ("${" + input + "}");
        }
        p_out << YAML::EndMap;
    }

    // Save outputs as separate section
    if (!p_node->outputs.empty())
    {
        p_out << YAML::Key << "outputs" << YAML::Value << YAML::BeginMap;
        for (const auto& output : p_node->outputs)
        {
            p_out << YAML::Key << output << YAML::Value
                  << ("${" + output + "}");
        }
        p_out << YAML::EndMap;
    }

    // Collect children to save
    std::vector<int> children_to_save;
    for (int child_id : p_node->children)
    {
        Node* child = findNode(child_id);
        if (child)
        {
            // When serializing subtree definitions, save all children
            // When serializing main tree, skip expanded inline subtree children
            bool skip_child = false;

            if (!is_subtree_definition && p_node->type == "SubTree" &&
                p_node->is_expanded)
            {
                // Check if this child is the expanded subtree root
                auto subtree_it = m_tree_views.find(p_node->subtree_reference);
                if (subtree_it != m_tree_views.end() &&
                    subtree_it->second.root_id == child_id)
                {
                    skip_child = true;
                }
            }

            if (!skip_child)
            {
                children_to_save.push_back(child_id);
            }
        }
    }

    if (!children_to_save.empty())
    {
        p_out << YAML::Key << "children" << YAML::Value << YAML::BeginSeq;
        for (int child_id : children_to_save)
        {
            Node* child = findNode(child_id);
            if (child)
            {
                // Propagate the is_subtree_definition flag to children
                serializeNodeToYaml(p_out, child, is_subtree_definition);
            }
        }
        p_out << YAML::EndSeq;
    }

    p_out << YAML::EndMap;
    p_out << YAML::EndMap;
}

// ----------------------------------------------------------------------------
int IDE::parseYamlNode(const YAML::Node& p_yaml_node, int p_parent_id)
{
    if (!p_yaml_node.IsMap())
        return -1;

    // The YAML node should have exactly one key which is the node type
    if (p_yaml_node.size() == 0)
        return -1;

    // Get the first (and should be only) key-value pair
    auto it = p_yaml_node.begin();
    std::string node_type = it->first.as<std::string>();
    YAML::Node node_data = it->second;

    // Create the editor node
    int node_id = getNextNodeId();
    std::string node_name = node_type;

    // Extract name if provided
    if (node_data["name"])
    {
        node_name = node_data["name"].as<std::string>();
    }

    IDE::Node editor_node;
    editor_node.id = node_id;
    editor_node.type = node_type;
    editor_node.name = node_name;
    editor_node.parent = p_parent_id;

    // Add to DFS order immediately after creation (for visualizer mode)
    m_dfs_node_order.push_back(node_id);

    // For SubTree nodes, extract reference
    if (editor_node.type == "SubTree" && node_data["reference"])
    {
        editor_node.subtree_reference =
            node_data["reference"].as<std::string>();
    }

    // Extract inputs
    if (node_data["inputs"])
    {
        YAML::Node inputs = node_data["inputs"];
        for (auto input_it = inputs.begin(); input_it != inputs.end();
             ++input_it)
        {
            std::string input_name = input_it->first.as<std::string>();
            editor_node.inputs.push_back(input_name);
        }
    }

    // Extract outputs
    if (node_data["outputs"])
    {
        YAML::Node outputs = node_data["outputs"];
        for (auto output_it = outputs.begin(); output_it != outputs.end();
             ++output_it)
        {
            std::string output_name = output_it->first.as<std::string>();
            editor_node.outputs.push_back(output_name);
        }
    }

    // Legacy: Extract parameters as inputs (for backward compatibility)
    if (node_data["parameters"])
    {
        YAML::Node params = node_data["parameters"];
        for (auto param_it = params.begin(); param_it != params.end();
             ++param_it)
        {
            std::string param_name = param_it->first.as<std::string>();
            // Only add if not already in inputs
            if (std::find(editor_node.inputs.begin(),
                          editor_node.inputs.end(),
                          param_name) == editor_node.inputs.end())
            {
                editor_node.inputs.push_back(param_name);
            }
        }
    }

    // Process children
    if (node_data["children"])
    {
        YAML::Node children = node_data["children"];
        if (children.IsSequence())
        {
            for (size_t i = 0; i < children.size(); ++i)
            {
                int child_id = parseYamlNode(children[i], node_id);
                if (child_id >= 0)
                {
                    editor_node.children.push_back(child_id);
                }
            }
        }
    }

    // Process child (for decorators)
    if (node_data["child"])
    {
        YAML::Node child = node_data["child"];
        if (child.IsSequence() && child.size() > 0)
        {
            int child_id = parseYamlNode(child[0], node_id);
            if (child_id >= 0)
            {
                editor_node.children.push_back(child_id);
            }
        }
    }

    // Add node to the map
    m_nodes.emplace(node_id, std::move(editor_node));

    return node_id;
}