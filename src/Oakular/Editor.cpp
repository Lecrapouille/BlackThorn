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

namespace {

//! \brief Canvas coordinate the auto-layout starts from, on both axes.
constexpr float c_layout_origin = 40.0f;
//! \brief Gap left between two siblings.
constexpr float c_layout_sibling_gap = 40.0f;
//! \brief Gap left between a parent and its children.
constexpr float c_layout_depth_gap = 60.0f;
//! \brief Gap left between two trees laid out side by side.
constexpr float c_layout_forest_gap = 100.0f;
//! \brief Node width assumed when no renderer can be asked.
constexpr float c_layout_fallback_width = 180.0f;
//! \brief Node height assumed when no renderer can be asked.
constexpr float c_layout_fallback_height = 80.0f;

} // namespace

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
    m_has_document = false;
    m_show_new_confirmation = false;
}

// ----------------------------------------------------------------------------
void Editor::newDocument()
{
    reset();
    m_has_document = true;

    // Give the document its "Main" view right away so that the canvas, and the
    // menus reading the layout direction, have something to work with.
    getCurrentTreeView();
}

// ----------------------------------------------------------------------------
bool Editor::save()
{
    if (!m_has_document)
        return false;

    if (m_behavior_tree_filepath.empty())
    {
        onFileDialogRequested.emit(FileDialog::Save);
        return false;
    }

    // Copied on purpose: saveToYaml assigns the path to the very member this
    // would otherwise alias.
    std::string const filepath = m_behavior_tree_filepath;
    return saveToYaml(filepath);
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

    // Collect the visible nodes of every root the view holds: its own root,
    // plus the nodes not attached to a parent yet, so that a node just created
    // is visible and can be linked.
    std::unordered_set<ID> visible_node_ids;
    for (ID root_id : collectViewRoots())
    {
        collectVisibleNodes(root_id, visible_node_ids);
    }

    // Non-const because the renderer writes the positions back when dragging.
    TreeView& current_view = getCurrentTreeView();

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
std::vector<Editor::ID> Editor::collectViewRoots()
{
    TreeView const& current_view = getCurrentTreeView();
    std::vector<ID> roots;

    if (findNode(current_view.root_id) != nullptr)
    {
        roots.push_back(current_view.root_id);
    }

    // A subtree tab only ever shows its own definition.
    if (current_view.is_subtree)
    {
        return roots;
    }

    // The definition of a SubTree is edited in its own tab, never inlined in
    // the main tree.
    std::unordered_set<ID> subtree_root_ids;
    for (auto const& [name, view] : m_tree_views)
    {
        if (view.is_subtree && view.root_id >= 0)
        {
            subtree_root_ids.insert(view.root_id);
        }
    }

    std::vector<ID> orphans;
    for (auto const& [id, node] : m_nodes)
    {
        if (node.parent == -1 && id != current_view.root_id &&
            subtree_root_ids.find(id) == subtree_root_ids.end())
        {
            orphans.push_back(id);
        }
    }

    // m_nodes is unordered: sort so that the auto-layout does not shuffle the
    // orphans from one call to the next.
    std::sort(orphans.begin(), orphans.end());
    roots.insert(roots.end(), orphans.begin(), orphans.end());

    return roots;
}

// ----------------------------------------------------------------------------
void Editor::collectVisibleNodes(ID p_root_id,
                                 std::unordered_set<ID>& p_visible_nodes)
{
    Node* node = findNode(p_root_id);
    if (!node)
        return;

    // Already collected: a malformed tree holding a cycle would otherwise
    // recurse forever.
    if (!p_visible_nodes.insert(p_root_id).second)
        return;

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

    // Adopt the node as the root while the view has none, so that the very
    // first node of a document, or the first one after the root was deleted,
    // gives the tree something to hang from.
    TreeView& current_view = getCurrentTreeView();
    if (findNode(current_view.root_id) == nullptr)
    {
        current_view.root_id = id;
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
    ID promoted_root = -1;

    Node* node = findNode(p_node_id);
    if (node)
    {
        // Deleting the root would leave the view without one, and hence
        // nothing to save: the first orphaned child takes over.
        if (getCurrentTreeView().root_id == p_node_id &&
            !node->children.empty())
        {
            promoted_root = node->children.front();
        }

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
    TreeView& current_view = getCurrentTreeView();
    if (current_view.root_id == p_node_id)
    {
        current_view.root_id = promoted_root;
    }
    current_view.node_positions.erase(p_node_id);

    if (m_selected_node_id == p_node_id)
    {
        m_selected_node_id = -1;
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

    // An arc always goes from a parent down to a child. Accepting the opposite
    // direction, from a node up to one of its own ancestors, would close a
    // cycle and leave a graph no longer serializable as a tree.
    if (p_from_node == p_to_node || isAncestorOf(p_to_node, p_from_node))
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

    // The former root just got a parent: the tree now hangs from whatever is
    // above it, otherwise saving and laying out would ignore the new levels.
    TreeView& current_view = getCurrentTreeView();
    if (current_view.root_id == p_to_node)
    {
        current_view.root_id = topmostAncestor(p_from_node);
    }

    m_is_modified = true;

    onLinkCreated.emit(p_from_node, p_to_node);
}

// ----------------------------------------------------------------------------
bool Editor::isAncestorOf(ID p_candidate, ID p_node)
{
    if (p_candidate < 0)
        return false;

    std::unordered_set<ID> walked;
    for (Node* node = findNode(p_node); node != nullptr;
         node = findNode(node->parent))
    {
        if (node->parent == p_candidate)
            return true;
        if (!walked.insert(node->id).second)
            break;
    }

    return false;
}

// ----------------------------------------------------------------------------
Editor::ID Editor::topmostAncestor(ID p_node)
{
    std::unordered_set<ID> walked;
    ID topmost = p_node;

    for (Node* node = findNode(p_node); node != nullptr;
         node = findNode(node->parent))
    {
        if (!walked.insert(node->id).second)
            break;
        topmost = node->id;
    }

    return topmost;
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
    std::vector<ID> const roots = collectViewRoots();
    if (roots.empty())
        return;

    std::unordered_map<ID, float> breadths;
    std::unordered_set<ID> placed;
    float breadth = c_layout_origin;

    // Every root of the view is laid out, the main tree first and the nodes not
    // attached to it yet next to it, each in its own lane.
    for (ID root_id : roots)
    {
        float const subtree_breadth = measureSubtree(root_id, breadths);
        if (subtree_breadth <= 0.0f)
            continue;

        placeSubtree(root_id, breadth, c_layout_origin, breadths, placed);
        breadth += subtree_breadth + c_layout_forest_gap;
    }

    syncNodePositionsFromView();
}

// ----------------------------------------------------------------------------
ImVec2 Editor::nodeSize(Node const& p_node) const
{
    if (m_renderer)
    {
        return m_renderer->measureNode(p_node);
    }
    return ImVec2(c_layout_fallback_width, c_layout_fallback_height);
}

// ----------------------------------------------------------------------------
float Editor::measureSubtree(ID p_node_id,
                             std::unordered_map<ID, float>& p_breadths)
{
    auto const known = p_breadths.find(p_node_id);
    if (known != p_breadths.end())
    {
        return known->second;
    }

    Node const* node = findNode(p_node_id);
    if (node == nullptr)
    {
        return 0.0f;
    }

    // Reserve the slot before recursing: a node reachable twice contributes
    // nothing the second time instead of looping forever.
    p_breadths[p_node_id] = 0.0f;

    bool const top_to_bottom =
        (getCurrentTreeView().layout_direction == LayoutDirection::TopToBottom);
    ImVec2 const size = nodeSize(*node);
    float const own_breadth = top_to_bottom ? size.x : size.y;

    float children_breadth = 0.0f;
    for (ID child_id : node->children)
    {
        float const child_breadth = measureSubtree(child_id, p_breadths);
        if (child_breadth <= 0.0f)
            continue;

        if (children_breadth > 0.0f)
        {
            children_breadth += c_layout_sibling_gap;
        }
        children_breadth += child_breadth;
    }

    float const breadth = std::max(own_breadth, children_breadth);
    p_breadths[p_node_id] = breadth;
    return breadth;
}

// ----------------------------------------------------------------------------
void Editor::placeSubtree(ID p_node_id,
                          float p_breadth,
                          float p_depth,
                          std::unordered_map<ID, float> const& p_breadths,
                          std::unordered_set<ID>& p_placed)
{
    Node const* node = findNode(p_node_id);
    if (node == nullptr)
        return;

    if (!p_placed.insert(p_node_id).second)
        return;

    auto breadthOf = [&p_breadths](ID p_id) {
        auto const it = p_breadths.find(p_id);
        return (it != p_breadths.end()) ? it->second : 0.0f;
    };

    bool const top_to_bottom =
        (getCurrentTreeView().layout_direction == LayoutDirection::TopToBottom);
    ImVec2 const size = nodeSize(*node);
    float const own_breadth = top_to_bottom ? size.x : size.y;
    float const own_depth = top_to_bottom ? size.y : size.x;
    float const slot = breadthOf(p_node_id);

    float const centered = p_breadth + (slot - own_breadth) * 0.5f;
    setNodePosition(p_node_id,
                    top_to_bottom ? ImVec2(centered, p_depth)
                                  : ImVec2(p_depth, centered));

    // Children of a leaf-shaped node take less room than the node itself: they
    // are centered under it rather than aligned on its left edge.
    float children_breadth = 0.0f;
    for (ID child_id : node->children)
    {
        float const child_breadth = breadthOf(child_id);
        if (child_breadth <= 0.0f)
            continue;

        if (children_breadth > 0.0f)
        {
            children_breadth += c_layout_sibling_gap;
        }
        children_breadth += child_breadth;
    }

    float child_breadth_cursor = p_breadth + (slot - children_breadth) * 0.5f;
    float const child_depth = p_depth + own_depth + c_layout_depth_gap;

    for (ID child_id : node->children)
    {
        float const child_breadth = breadthOf(child_id);
        if (child_breadth <= 0.0f)
            continue;

        placeSubtree(
            child_id, child_breadth_cursor, child_depth, p_breadths, p_placed);
        child_breadth_cursor += child_breadth + c_layout_sibling_gap;
    }
}

// ----------------------------------------------------------------------------
void Editor::syncNodePositionsFromView()
{
    TreeView const& current_view = getCurrentTreeView();
    for (auto const& [id, position] : current_view.node_positions)
    {
        auto node_it = m_nodes.find(id);
        if (node_it != m_nodes.end())
        {
            node_it->second.position = position;
        }
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
bool Editor::loadFromYaml(std::string const& p_filepath)
{
    auto doc = bt::YamlDocument::parseFile(p_filepath);
    if (!doc)
    {
        std::cerr << "YAML parsing error: " << doc.getError() << std::endl;
        return false;
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
    m_has_document = false;

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
            return false;
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
        return false;
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

    // Auto-layout the nodes of every view. The layout works on the active view,
    // hence the round trip through m_active_tree_name.
    std::string const saved_active_name = m_active_tree_name;
    std::vector<std::string> view_names;
    view_names.reserve(m_tree_views.size());
    for (auto const& [name, view] : m_tree_views)
    {
        view_names.push_back(name);
    }

    for (std::string const& name : view_names)
    {
        m_active_tree_name = name;
        autoLayoutNodes();
    }

    m_active_tree_name = saved_active_name;

    m_is_modified = false;
    m_has_document = true;
    m_behavior_tree_filepath = p_filepath;

    return true;
}

// ----------------------------------------------------------------------------
bool Editor::loadFromYamlString(std::string const& p_yaml_content)
{
    auto doc = bt::YamlDocument::parseText(p_yaml_content);
    if (!doc)
    {
        std::cerr << "YAML parsing error: " << doc.getError() << std::endl;
        return false;
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
            return false;
        }

        std::string tree_name = "Visualizer";

        m_tree_views[tree_name] = {
            tree_name, false, root_id, LayoutDirection::TopToBottom, {}};
        m_active_tree_name = tree_name;
    }
    else
    {
        std::cerr << "No BehaviorTree section found in YAML" << std::endl;
        return false;
    }

    autoLayoutNodes();

    m_is_modified = false;

    return true;
}

// ----------------------------------------------------------------------------
bool Editor::saveToYaml(std::string const& p_filepath)
{
    ID root_id = getCurrentTreeView().root_id;
    Node* root = findNode(root_id);
    if (root == nullptr)
    {
        std::cerr << "Nothing to save: the tree has no root node" << std::endl;
        return false;
    }

    // A behavior tree hangs from a single root: nodes left floating around
    // cannot be written down. Say so rather than dropping them silently.
    std::unordered_set<ID> saved_nodes;
    collectVisibleNodes(root_id, saved_nodes);
    for (auto const& [name, view] : m_tree_views)
    {
        if (view.is_subtree)
        {
            collectVisibleNodes(view.root_id, saved_nodes);
        }
    }

    std::size_t const dangling = m_nodes.size() - saved_nodes.size();
    if (dangling > 0U)
    {
        std::cerr << "Warning: " << dangling
                  << " node(s) are not connected to the root and will not be "
                     "saved"
                  << std::endl;
    }

    std::ofstream file(p_filepath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file for writing: " << p_filepath
                  << std::endl;
        return false;
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
    m_has_document = true;
    m_behavior_tree_filepath = p_filepath;

    return true;
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

    // The "- " prefix shifts the type key one indentation level to the right,
    // so the keys nested under it must follow. Without this the keys land at
    // the level of the type itself and YAML reads them as its siblings: the
    // name and, worse, the whole children list of every node but the root are
    // lost on reload.
    int const inner = p_indent + (p_sequence_item ? 2 : 1);
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
