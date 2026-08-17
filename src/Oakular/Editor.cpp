/**
 * @file Editor.cpp
 * @brief Oakular - Embeddable behavior tree editor: tree model and I/O.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/BlackThorn
 */

#include "Editor.hpp"
#include "TreeRenderer.hpp"

#if defined(OAKULAR_HAS_SERVER)
#    include "Server.hpp"
#endif

#include "BlackThorn/Blackboard/Serializer.hpp"
#include "BlackThorn/Builder/Yaml.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace oakular {

// ----------------------------------------------------------------------------
//! \brief Extract the filename without extension from a file path.
//! \param[in] p_filepath The full file path.
//! \return The filename without extension, or empty string if extraction fails.
// ----------------------------------------------------------------------------
static std::string
extractFileNameWithoutExtension(std::string const& p_filepath)
{
    if (p_filepath.empty())
        return "";

    try
    {
        std::filesystem::path path(p_filepath);
        std::string filename = path.stem().string();
        return filename.empty() ? "" : filename;
    }
    catch (...)
    {
        // Fallback: manual extraction if filesystem fails
        size_t last_slash = p_filepath.find_last_of("/\\");
        size_t start = (last_slash == std::string::npos) ? 0 : last_slash + 1;
        size_t last_dot = p_filepath.find_last_of('.');
        size_t end = (last_dot == std::string::npos || last_dot < start)
                         ? p_filepath.length()
                         : last_dot;
        return p_filepath.substr(start, end - start);
    }
}

// ----------------------------------------------------------------------------
static void appendYamlIndent(std::ostringstream& p_out, int p_indent)
{
    p_out << std::string(static_cast<std::size_t>(p_indent) * 2U, ' ');
}

// ----------------------------------------------------------------------------
static void appendYamlScalar(std::ostringstream& p_out,
                             std::string const& p_value)
{
    bool needs_quote = p_value.empty();
    if (!needs_quote)
    {
        for (char ch : p_value)
        {
            if (ch == ':' || ch == '#' || ch == '\n' || ch == '"' || ch == '\'')
            {
                needs_quote = true;
                break;
            }
        }
        if (!needs_quote &&
            (p_value.front() == ' ' || p_value.back() == ' ' ||
             p_value == "true" || p_value == "false"))
        {
            needs_quote = true;
        }
    }

    if (needs_quote)
    {
        p_out << '"';
        for (char ch : p_value)
        {
            if (ch == '"' || ch == '\\')
            {
                p_out << '\\';
            }
            p_out << ch;
        }
        p_out << '"';
    }
    else
    {
        p_out << p_value;
    }
}

// ----------------------------------------------------------------------------
Editor::Editor() : Editor(nullptr) {}

// ----------------------------------------------------------------------------
Editor::Editor(bt::Blackboard::Ptr p_blackboard)
{
    registerBuiltinNodeTypes();
    m_renderer = std::make_unique<TreeRenderer>();
    m_blackboard =
        p_blackboard ? std::move(p_blackboard)
                     : std::make_shared<bt::Blackboard>();
}

// ----------------------------------------------------------------------------
Editor::~Editor()
{
    teardown();
}

// ----------------------------------------------------------------------------
bool Editor::setup()
{
    if (!m_renderer)
    {
        m_renderer = std::make_unique<TreeRenderer>();
    }
    reset();
    return true;
}

// ----------------------------------------------------------------------------
void Editor::teardown()
{
#if defined(OAKULAR_HAS_SERVER)
    if (m_server)
    {
        m_server->stop();
        m_server.reset();
    }
#endif

    if (m_renderer)
    {
        m_renderer->shutdown();
        m_renderer.reset();
    }
}

// ----------------------------------------------------------------------------
void Editor::update([[maybe_unused]] float p_dt)
{
    if (m_mode != Mode::Visualizer)
        return;

#if defined(OAKULAR_HAS_SERVER)
    if (!m_server)
        return;

    m_server->update();

    if (!m_server->isConnected())
    {
        // Forget the tree so that it is reloaded on the next connection.
        m_dfs_node_order.clear();
        return;
    }

    if (!m_server->hasReceivedTree())
        return;

    if (m_dfs_node_order.empty())
    {
        loadFromYamlString(m_server->getYamlData());
    }

    if (m_server->hasStateUpdate())
    {
        for (auto& [node_id, node] : m_nodes)
        {
            node.runtime_status = m_server->getNodeState(node_id);
        }
        m_server->clearStateUpdate();
    }
#endif
}

// ----------------------------------------------------------------------------
void Editor::reset()
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
    m_show_palettes.position = ImVec2(0, 0);
    m_show_palettes.canvas_position = ImVec2(0, 0);
    m_node_edit.initialized = false;
    m_blackboard_panel.edit_buffers.clear();
    m_blackboard = std::make_shared<bt::Blackboard>();
}

// ----------------------------------------------------------------------------
void Editor::setBlackboard(bt::Blackboard::Ptr p_blackboard)
{
    if (!p_blackboard)
        return;

    m_blackboard = std::move(p_blackboard);
    m_blackboard_panel.edit_buffers.clear();
}

// ----------------------------------------------------------------------------
void Editor::registerBuiltinNodeTypes()
{
    m_node_types = {{"Action", "Leaf", true},
                    {"Condition", "Leaf", true},
                    {"Success", "Leaf", false},
                    {"Failure", "Leaf", false},
                    {"Sequence", "Composite", false},
                    {"Selector", "Composite", false},
                    {"Parallel", "Composite", false},
                    {"Inverter", "Decorator", false},
                    {"Repeater", "Decorator", false},
                    {"SubTree", "SubTree", true}};
}

// ----------------------------------------------------------------------------
void Editor::registerNodeType(std::string const& p_name,
                              std::string const& p_category,
                              bool const p_can_have_ports)
{
    if (p_name.empty())
        return;

    auto it = std::find_if(m_node_types.begin(),
                           m_node_types.end(),
                           [&p_name](NodeType const& p_type)
                           { return p_type.name == p_name; });
    if (it != m_node_types.end())
    {
        it->category = p_category;
        it->can_have_ports = p_can_have_ports;
        return;
    }

    m_node_types.push_back({p_name, p_category, p_can_have_ports});
}

// ----------------------------------------------------------------------------
bool Editor::canHaveBlackboardPorts(std::string const& p_type) const
{
    auto it = std::find_if(m_node_types.begin(),
                           m_node_types.end(),
                           [&p_type](NodeType const& p_node_type)
                           { return p_node_type.name == p_type; });
    return (it != m_node_types.end()) && it->can_have_ports;
}

#if defined(OAKULAR_HAS_SERVER)

// ----------------------------------------------------------------------------
void Editor::attachServer(std::shared_ptr<Server> p_server)
{
    if (m_server && m_server != p_server)
    {
        m_server->stop();
    }
    m_server = std::move(p_server);
}

#endif // OAKULAR_HAS_SERVER

// ----------------------------------------------------------------------------
void Editor::setMode(Mode const p_mode)
{
    if (m_mode == p_mode)
        return;

    m_mode = p_mode;
    switch (m_mode)
    {
        case Mode::Visualizer:
            // Clear the current tree when entering visualizer mode
            reset();
#if defined(OAKULAR_HAS_SERVER)
            if (m_server && !m_server->isConnected())
            {
                m_server->start();
            }
#endif
            break;
        case Mode::Creation:
#if defined(OAKULAR_HAS_SERVER)
            if (m_server)
            {
                m_server->stop();
            }
#endif
            break;
        default:
            std::cout << "Unknown editor mode" << std::endl;
            break;
    }
}

// ----------------------------------------------------------------------------
void Editor::drawBehaviorTree()
{
    if (!m_renderer)
        return;

    // Get current view (non-const because we'll modify node_positions)
    TreeView& current_view = getCurrentTreeView();

    // Collect visible nodes from current root
    std::unordered_set<ID> visible_node_ids;
    ID current_root_id = current_view.root_id;

    // Nothing to draw until the view owns a root node.
    if (current_root_id < 0)
    {
        return;
    }

    collectVisibleNodes(current_root_id, visible_node_ids);

    // Only include orphan nodes when viewing the main tree, not subtrees
    // This allows new unconnected nodes to be visible in the main tree
    if (!current_view.is_subtree)
    {
        // Build set of subtree root IDs (these should not appear in main tree)
        std::unordered_set<ID> subtree_root_ids;
        for (auto const& [name, view] : m_tree_views)
        {
            if (view.is_subtree && view.root_id >= 0)
            {
                subtree_root_ids.insert(view.root_id);
            }
        }

        // Include orphan nodes (nodes without parents that aren't the root)
        // BUT exclude subtree definition roots
        for (auto const& [id, node] : m_nodes)
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
    std::unordered_map<ID, Node> visible_nodes;

    // Sync positions from current view's node_positions to nodes (for renderer)
    // Also update m_nodes so positions are in sync
    for (auto const& [id, node] : m_nodes)
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
    ID link_id = 0;
    for (auto const& [id, node] : visible_nodes)
    {
        for (ID child_id : node.children)
        {
            if (visible_node_ids.count(child_id) > 0)
            {
                visible_links.push_back({link_id++, id, child_id});
            }
        }
    }

    // Render the graph
    bool const is_edit_mode = (m_mode == Mode::Creation);
    LayoutDirection layout_dir = getCurrentTreeView().layout_direction;
    int layout_dir_int = static_cast<int>(layout_dir);
    m_renderer->drawBehaviorTree(visible_nodes,
                                 visible_links,
                                 layout_dir_int,
                                 m_blackboard.get(),
                                 !is_edit_mode);

    // Save positions back to current view's node_positions (in case the
    // renderer modified them)
    for (auto& [id, node] : visible_nodes)
    {
        current_view.node_positions[id] = node.position;
    }
}

// ----------------------------------------------------------------------------
void Editor::collectVisibleNodes(ID p_root_id,
                                 std::unordered_set<ID>& p_visible_nodes)
{
    Node* node = findNode(p_root_id);
    if (!node)
        return;

    // Add this node to visible set
    p_visible_nodes.insert(p_root_id);

    // Process children
    for (ID child_id : node->children)
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
        collectVisibleNodes(child_id, p_visible_nodes);
    }
}

// ----------------------------------------------------------------------------
Editor::ID Editor::addNode(std::string const& p_type, std::string const& p_name)
{
    ID id = getNextNodeId();
    Node node;
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

    return id;
}

// ----------------------------------------------------------------------------
void Editor::addNodeAndLink(std::string const& p_type,
                            std::string const& p_name)
{
    ID new_node_id = addNode(p_type, p_name);

    // If we have a pending link, complete it now
    if (m_pending_link_from_node >= 0)
    {
        createLink(m_pending_link_from_node, new_node_id);
        m_pending_link_from_node = -1;
    }

    ImGui::CloseCurrentPopup();
}

// ----------------------------------------------------------------------------
void Editor::deleteNode(ID const p_node_id)
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
        for (ID child_id : node->children)
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
void Editor::createLink(ID const p_from_node, ID const p_to_node)
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

    onLinkCreated.emit(p_from_node, p_to_node);
}

// ----------------------------------------------------------------------------
void Editor::deleteLink(ID const p_from_node, ID const p_to_node)
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
Editor::Node* Editor::findNode(ID const p_id)
{
    auto it = m_nodes.find(p_id);
    return it != m_nodes.end() ? &it->second : nullptr;
}

// ----------------------------------------------------------------------------
ImVec2 Editor::getNodePosition(ID p_node_id)
{
    TreeView& current_view = getCurrentTreeView();
    auto it = current_view.node_positions.find(p_node_id);
    if (it != current_view.node_positions.end())
    {
        return it->second;
    }
    return ImVec2(0, 0); // Default position
}

// ----------------------------------------------------------------------------
void Editor::setNodePosition(ID p_node_id, ImVec2 p_position)
{
    TreeView& current_view = getCurrentTreeView();
    current_view.node_positions[p_node_id] = p_position;
}

// ----------------------------------------------------------------------------
Editor::TreeView& Editor::getCurrentTreeView()
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
Editor::TreeView* Editor::findTreeViewByRootId(ID p_root_id)
{
    for (auto& [name, view] : m_tree_views)
    {
        if (view.root_id == p_root_id)
        {
            return &view;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
void Editor::autoLayoutNodes()
{
    ID root_id = getCurrentTreeView().root_id;
    if (root_id < 0)
        return;

    Node* root = findNode(root_id);
    if (!root)
        return;

    float max_extent = 0;
    layoutNodeRecursive(root, 100.0f, 100.0f, max_extent);
}

// ----------------------------------------------------------------------------
void Editor::layoutNodeRecursive(Node* p_node,
                                 float p_x,
                                 float p_y,
                                 float& p_max_extent)
{
    if (!p_node)
        return;

    // Groot-style layout with proper spacing
    constexpr float NODE_HORIZONTAL_SPACING = 40.0f;
    constexpr float NODE_VERTICAL_SPACING = 60.0f;
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
            float center_y =
                (min_child_y + max_child_y) / 2.0f - node_size.y / 2.0f;
            ImVec2 pos = ImVec2(p_x, std::max(p_y, center_y));
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
            float center_x =
                (min_child_x + max_child_x) / 2.0f - node_size.x / 2.0f;
            ImVec2 pos = ImVec2(std::max(p_x, center_x), p_y);
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
void Editor::toggleSubTreeExpansion(ID const p_node_id)
{
    Node* node = findNode(p_node_id);
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
bool Editor::expandSubTree(Node* p_subtree_node)
{
    if (!p_subtree_node || p_subtree_node->subtree_reference.empty())
        return false;

    // Find the SubTree definition
    auto it = m_tree_views.find(p_subtree_node->subtree_reference);
    if (it == m_tree_views.end() || !it->second.is_subtree)
        return false;

    ID subtree_root_id = it->second.root_id;
    Node* subtree_root = findNode(subtree_root_id);
    if (!subtree_root)
        return false;

    // Link the SubTree root as a child of the SubTree node
    if (std::find(p_subtree_node->children.begin(),
                  p_subtree_node->children.end(),
                  subtree_root_id) == p_subtree_node->children.end())
    {
        p_subtree_node->children.push_back(subtree_root_id);
        subtree_root->parent = p_subtree_node->id;
    }

    // Relayout after expansion
    autoLayoutNodes();
    return true;
}

// ----------------------------------------------------------------------------
bool Editor::collapseSubTree(Node* p_subtree_node)
{
    if (!p_subtree_node || p_subtree_node->subtree_reference.empty())
        return false;

    // Find the SubTree definition
    auto it = m_tree_views.find(p_subtree_node->subtree_reference);
    if (it == m_tree_views.end() || !it->second.is_subtree)
        return false;

    ID subtree_root_id = it->second.root_id;

    // Remove the link between SubTree node and its expanded content
    auto child_it = std::find(p_subtree_node->children.begin(),
                              p_subtree_node->children.end(),
                              subtree_root_id);
    if (child_it != p_subtree_node->children.end())
    {
        p_subtree_node->children.erase(child_it);
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
void Editor::loadFromYaml(std::string const& p_filepath)
{
    auto doc = bt::YamlDocument::parseFile(p_filepath);
    if (!doc)
    {
        std::cerr << "YAML parsing error: " << doc.getError() << std::endl;
        return;
    }

    bt::YamlDocument yaml_doc = doc.moveValue();
    bt::YamlNode const root = yaml_doc.root();

    // Clear existing data
    m_nodes.clear();
    m_tree_views.clear();
    m_unique_node_id = 1;
    m_selected_node_id = -1;
    m_active_tree_name.clear();
    m_dfs_node_order.clear();

    // Create a fresh blackboard and parse the Blackboard section
    m_blackboard = std::make_shared<bt::Blackboard>();
    m_blackboard_panel.edit_buffers.clear();
    if (root.hasKey("Blackboard"))
    {
        bt::BlackboardSerializer::load(*m_blackboard, root.child("Blackboard"));
    }

    // Parse the BehaviorTree section
    if (root.hasKey("BehaviorTree"))
    {
        bt::YamlNode tree_node = root.child("BehaviorTree");
        ID root_id = parseYamlNode(tree_node, -1);

        if (root_id < 0)
        {
            std::cerr << "Failed to parse BehaviorTree section" << std::endl;
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
    if (root.hasKey("SubTrees"))
    {
        root.child("SubTrees")
            .forEachMap(
                [&](std::string_view p_subtree_name, bt::YamlNode p_subtree_def)
                {
                    std::string subtree_name(p_subtree_name);
                    ID subtree_root_id = parseYamlNode(p_subtree_def, -1);

                    if (subtree_root_id < 0)
                    {
                        std::cerr << "Failed to parse SubTree: " << subtree_name
                                  << std::endl;
                        return;
                    }

                    m_tree_views[subtree_name] = {subtree_name,
                                                  true,
                                                  subtree_root_id,
                                                  LayoutDirection::TopToBottom,
                                                  {}};
                });
    }

    // Auto-layout the nodes for each tree view
    std::string saved_active_name = m_active_tree_name;

    for (auto& [name, view] : m_tree_views)
    {
        m_active_tree_name = name;
        autoLayoutNodes();

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
                node_it->second.position = pos_it->second;
            }
        }
    }

    m_active_tree_name = saved_active_name;

    m_is_modified = false;
    m_behavior_tree_filepath = p_filepath;
}

// ----------------------------------------------------------------------------
void Editor::loadFromYamlString(std::string const& p_yaml_content)
{
    auto doc = bt::YamlDocument::parseText(p_yaml_content);
    if (!doc)
    {
        std::cerr << "YAML parsing error: " << doc.getError() << std::endl;
        return;
    }

    bt::YamlDocument yaml_doc = doc.moveValue();
    bt::YamlNode const root = yaml_doc.root();

    m_nodes.clear();
    m_tree_views.clear();
    m_unique_node_id = 1;
    m_selected_node_id = -1;
    m_active_tree_name.clear();
    m_dfs_node_order.clear();

    m_blackboard = std::make_shared<bt::Blackboard>();
    m_blackboard_panel.edit_buffers.clear();
    if (root.hasKey("Blackboard"))
    {
        bt::BlackboardSerializer::load(*m_blackboard, root.child("Blackboard"));
    }

    if (root.hasKey("SubTrees"))
    {
        root.child("SubTrees")
            .forEachMap(
                [&](std::string_view p_subtree_name, bt::YamlNode p_subtree_def)
                {
                    std::string subtree_name(p_subtree_name);
                    ID subtree_root_id = parseYamlNode(p_subtree_def, -1);

                    if (subtree_root_id < 0)
                    {
                        std::cerr << "Failed to parse SubTree: " << subtree_name
                                  << std::endl;
                        return;
                    }

                    m_tree_views[subtree_name] = {subtree_name,
                                                  true,
                                                  subtree_root_id,
                                                  LayoutDirection::TopToBottom,
                                                  {}};
                });
    }

    if (root.hasKey("BehaviorTree"))
    {
        bt::YamlNode tree_node = root.child("BehaviorTree");
        ID root_id = parseYamlNode(tree_node, -1);

        if (root_id < 0)
        {
            std::cerr << "Failed to parse BehaviorTree section" << std::endl;
            return;
        }

        std::string tree_name = "Visualizer";

        m_tree_views[tree_name] = {
            tree_name, false, root_id, LayoutDirection::TopToBottom, {}};
        m_active_tree_name = tree_name;
    }
    else
    {
        std::cerr << "No BehaviorTree section found in YAML" << std::endl;
        return;
    }

    autoLayoutNodes();

    m_is_modified = false;
}

// ----------------------------------------------------------------------------
void Editor::saveToYaml(std::string const& p_filepath)
{
    ID root_id = getCurrentTreeView().root_id;
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

    std::ostringstream out;

    if (m_blackboard)
    {
        std::string const blackboard_yaml =
            bt::BlackboardSerializer::dump(*m_blackboard);
        if (!blackboard_yaml.empty())
        {
            out << "Blackboard:\n";
            std::istringstream lines(blackboard_yaml);
            std::string line;
            while (std::getline(lines, line))
            {
                out << "  " << line << '\n';
            }
        }
    }

    out << "BehaviorTree:\n";
    serializeNodeToYaml(out, root, 1, false, false);

    bool has_subtrees = false;
    for (auto const& [name, view] : m_tree_views)
    {
        if (view.is_subtree)
        {
            has_subtrees = true;
            break;
        }
    }

    if (has_subtrees)
    {
        out << "SubTrees:\n";

        for (auto const& [name, view] : m_tree_views)
        {
            if (view.is_subtree && view.root_id >= 0)
            {
                Node* subtree_root = findNode(view.root_id);
                if (subtree_root)
                {
                    out << "  " << name << ":\n";
                    serializeNodeToYaml(out, subtree_root, 2, true, false);
                }
            }
        }
    }

    file << out.str();
    file.close();

    m_is_modified = false;
    m_behavior_tree_filepath = p_filepath;
}

// ============================================================================
// Tree Conversion
// ============================================================================

// ----------------------------------------------------------------------------
void Editor::buildTreeFromNodes()
{
    // Saving currently walks the graphical Node map directly, so there is
    // nothing to build here yet.
}

// ----------------------------------------------------------------------------
void Editor::buildNodesFromTree(bt::Node& p_root)
{
    buildNodesFromTreeRecursive(p_root, -1);
    autoLayoutNodes();
}

// ----------------------------------------------------------------------------
Editor::ID Editor::buildNodesFromTreeRecursive(bt::Node& p_node, ID p_parent_id)
{
    ID node_id = getNextNodeId();
    Node editor_node;
    editor_node.id = node_id;
    editor_node.type = p_node.typeName();
    editor_node.name = p_node.name;
    editor_node.parent = p_parent_id;

    if (auto composite = dynamic_cast<bt::Composite*>(&p_node))
    {
        for (uint32_t child_index : composite->childIndices())
        {
            ID child_id = buildNodesFromTreeRecursive(
                composite->ownerTree()->node(child_index), node_id);
            if (child_id >= 0)
            {
                editor_node.children.push_back(child_id);
            }
        }
    }
    else if (auto decorator = dynamic_cast<bt::Decorator*>(&p_node))
    {
        if (decorator->hasChild())
        {
            ID child_id =
                buildNodesFromTreeRecursive(decorator->childNode(), node_id);
            if (child_id >= 0)
            {
                editor_node.children.push_back(child_id);
            }
        }
    }

    m_nodes.emplace(node_id, std::move(editor_node));

    if (p_parent_id < 0)
    {
        getCurrentTreeView().root_id = node_id;
    }

    return node_id;
}

// ----------------------------------------------------------------------------
void Editor::serializeNodeToYaml(std::ostringstream& p_out,
                                 Node* p_node,
                                 int p_indent,
                                 bool p_is_subtree_definition,
                                 bool p_sequence_item)
{
    if (!p_node)
        return;

    appendYamlIndent(p_out, p_indent);
    if (p_sequence_item)
    {
        p_out << "- ";
    }
    p_out << p_node->type << ":\n";

    int const inner = p_indent + 1;
    appendYamlIndent(p_out, inner);
    p_out << "name: ";
    appendYamlScalar(p_out, p_node->name);
    p_out << '\n';

    if (p_node->type == "SubTree" && !p_node->subtree_reference.empty() &&
        !p_is_subtree_definition)
    {
        appendYamlIndent(p_out, inner);
        p_out << "reference: ";
        appendYamlScalar(p_out, p_node->subtree_reference);
        p_out << '\n';
    }

    if (!p_node->inputs.empty())
    {
        appendYamlIndent(p_out, inner);
        p_out << "inputs:\n";
        for (auto const& input : p_node->inputs)
        {
            appendYamlIndent(p_out, inner + 1);
            p_out << input << ": ";
            appendYamlScalar(p_out, "${" + input + "}");
            p_out << '\n';
        }
    }

    if (!p_node->outputs.empty())
    {
        appendYamlIndent(p_out, inner);
        p_out << "outputs:\n";
        for (auto const& output : p_node->outputs)
        {
            appendYamlIndent(p_out, inner + 1);
            p_out << output << ": ";
            appendYamlScalar(p_out, "${" + output + "}");
            p_out << '\n';
        }
    }

    std::vector<ID> children_to_save;
    for (ID child_id : p_node->children)
    {
        Node* child = findNode(child_id);
        if (child)
        {
            bool skip_child = false;

            if (!p_is_subtree_definition && p_node->type == "SubTree" &&
                p_node->is_expanded)
            {
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
        appendYamlIndent(p_out, inner);
        p_out << "children:\n";
        for (ID child_id : children_to_save)
        {
            Node* child = findNode(child_id);
            if (child)
            {
                serializeNodeToYaml(
                    p_out, child, inner + 1, p_is_subtree_definition, true);
            }
        }
    }
}

// ----------------------------------------------------------------------------
Editor::ID Editor::parseYamlNode(bt::YamlNode const& p_yaml_node,
                                 ID p_parent_id)
{
    if (!p_yaml_node.isMap())
        return -1;

    auto const [node_type, node_data] = p_yaml_node.typeEntry();
    if (node_type.empty() || !node_data.valid())
        return -1;

    ID node_id = getNextNodeId();
    std::string node_name = node_type;

    if (node_data.hasKey("name"))
    {
        node_name = node_data.child("name").scalar();
    }

    Node editor_node;
    editor_node.id = node_id;
    editor_node.type = node_type;
    editor_node.name = node_name;
    editor_node.parent = p_parent_id;

    m_dfs_node_order.push_back(node_id);

    if (editor_node.type == "SubTree" && node_data.hasKey("reference"))
    {
        editor_node.subtree_reference = node_data.child("reference").scalar();
    }

    if (node_data.hasKey("inputs"))
    {
        node_data.child("inputs").forEachMap(
            [&](std::string_view p_input_name, bt::YamlNode)
            { editor_node.inputs.emplace_back(p_input_name); });
    }

    if (node_data.hasKey("outputs"))
    {
        node_data.child("outputs").forEachMap(
            [&](std::string_view p_output_name, bt::YamlNode)
            { editor_node.outputs.emplace_back(p_output_name); });
    }

    if (node_data.hasKey("parameters"))
    {
        node_data.child("parameters")
            .forEachMap(
                [&](std::string_view p_param_name, bt::YamlNode)
                {
                    std::string param_name(p_param_name);
                    if (std::find(editor_node.inputs.begin(),
                                  editor_node.inputs.end(),
                                  param_name) == editor_node.inputs.end())
                    {
                        editor_node.inputs.push_back(param_name);
                    }
                });
    }

    if (node_data.hasKey("children"))
    {
        bt::YamlNode children = node_data.child("children");
        if (children.isSeq())
        {
            children.forEachSeq(
                [&](bt::YamlNode p_child)
                {
                    ID child_id = parseYamlNode(p_child, node_id);
                    if (child_id >= 0)
                    {
                        editor_node.children.push_back(child_id);
                    }
                });
        }
    }

    if (node_data.hasKey("child"))
    {
        bt::YamlNode child = node_data.child("child");
        if (child.isSeq() && child.size() > 0)
        {
            ID child_id = parseYamlNode(child.child(0), node_id);
            if (child_id >= 0)
            {
                editor_node.children.push_back(child_id);
            }
        }
    }

    m_nodes.emplace(node_id, std::move(editor_node));

    return node_id;
}

} // namespace oakular
