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
#include <array>
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
        if (!needs_quote && (p_value.front() == ' ' || p_value.back() == ' ' ||
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
    publishChildPolicy();
    m_blackboard = p_blackboard ? std::move(p_blackboard)
                                : std::make_shared<bt::Blackboard>();
}

// ----------------------------------------------------------------------------
void Editor::publishChildPolicy()
{
    if (!m_renderer)
        return;

    m_renderer->setChildPolicy([this](std::string const& p_type) {
        std::string const category = nodeCategory(p_type);
        return category == "Composite" || category == "Decorator" ||
               category == "SubTree" || category == "Custom";
    });
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
        publishChildPolicy();
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
    m_selected_nodes.clear();
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
std::string Editor::documentTitle() const
{
    if (!m_has_document)
        return "";

    std::string title = m_behavior_tree_filepath.empty()
                            ? std::string("Untitled")
                            : std::filesystem::path(m_behavior_tree_filepath)
                                  .filename()
                                  .string();
    if (m_is_modified)
    {
        title += " *";
    }
    return title;
}

// ----------------------------------------------------------------------------
bt::Blackboard::Ptr Editor::activeBlackboard()
{
    TreeView& view = getCurrentTreeView();
    if (!view.blackboard)
    {
        // A subtree reads what it does not define from the tree holding it,
        // exactly as the nested blackboard bt::Builder creates at runtime.
        view.blackboard = view.is_subtree && m_blackboard
                              ? m_blackboard->createChild()
                              : m_blackboard;
    }
    return view.blackboard;
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
    // Kept aligned with the creators bt::Builder knows, so that a tree written
    // here always names types the engine can instantiate, and so that the
    // number of children allowed per type is known when saving.
    m_node_types = {{"Action", "Leaf", true},
                    {"Condition", "Leaf", true},
                    {"Success", "Leaf", false},
                    {"Failure", "Leaf", false},
                    {"Wait", "Leaf", false},
                    {"SetBlackboard", "Leaf", false},
                    {"Sequence", "Composite", false},
                    {"Selector", "Composite", false},
                    {"Parallel", "Composite", false},
                    {"Inverter", "Decorator", false},
                    {"Repeat", "Decorator", true},
                    {"UntilSuccess", "Decorator", false},
                    {"UntilFailure", "Decorator", false},
                    {"ForceSuccess", "Decorator", false},
                    {"ForceFailure", "Decorator", false},
                    {"RunOnce", "Decorator", false},
                    {"Timeout", "Decorator", false},
                    {"Delay", "Decorator", false},
                    {"Cooldown", "Decorator", false},
                    {"SubTree", "SubTree", true}};
}

// ----------------------------------------------------------------------------
std::string Editor::nodeCategory(std::string const& p_type) const
{
    auto it = std::find_if(
        m_node_types.begin(),
        m_node_types.end(),
        [&p_type](NodeType const& p_entry) { return p_entry.name == p_type; });
    return it != m_node_types.end() ? it->category : std::string();
}

// ----------------------------------------------------------------------------
bool Editor::isDecoratorType(std::string const& p_type) const
{
    return nodeCategory(p_type) == "Decorator";
}

// ----------------------------------------------------------------------------
void Editor::registerNodeType(std::string const& p_name,
                              std::string const& p_category,
                              bool const p_can_have_ports)
{
    if (p_name.empty())
        return;

    auto it = std::find_if(
        m_node_types.begin(),
        m_node_types.end(),
        [&p_name](NodeType const& p_type) { return p_type.name == p_name; });
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
                           [&p_type](NodeType const& p_node_type) {
                               return p_node_type.name == p_type;
                           });
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

    // Render the graph. The renderer holds no selection of its own: it draws
    // the one the editor owns.
    bool const is_edit_mode = (m_mode == Mode::Creation);
    LayoutDirection layout_dir = current_view.layout_direction;
    int layout_dir_int = static_cast<int>(layout_dir);
    m_renderer->setSelection(m_selected_nodes);
    m_renderer->drawBehaviorTree(visible_nodes,
                                 visible_links,
                                 layout_dir_int,
                                 activeBlackboard().get(),
                                 !is_edit_mode);

    // Dragging one node of a selection drags the whole group, so the user may
    // move a branch without losing the relative placement inside it.
    ImVec2 drag_delta(0.0f, 0.0f);
    if (m_selected_nodes.size() > 1U)
    {
        for (auto const& [id, node] : visible_nodes)
        {
            if (m_selected_nodes.count(id) == 0U)
                continue;

            auto const previous = current_view.node_positions.find(id);
            if (previous == current_view.node_positions.end())
                continue;

            ImVec2 const delta(node.position.x - previous->second.x,
                               node.position.y - previous->second.y);
            if ((delta.x != 0.0f) || (delta.y != 0.0f))
            {
                drag_delta = delta;
                break;
            }
        }
    }

    // Save positions back to current view's node_positions (in case the
    // renderer modified them)
    bool moved = false;
    for (auto& [id, node] : visible_nodes)
    {
        ImVec2 position = node.position;
        auto const previous = current_view.node_positions.find(id);
        bool const known = (previous != current_view.node_positions.end());

        // A selected node the renderer left alone follows the one it moved.
        if (known && (m_selected_nodes.count(id) > 0U) &&
            (position.x == previous->second.x) &&
            (position.y == previous->second.y))
        {
            position.x += drag_delta.x;
            position.y += drag_delta.y;
        }

        if (known && ((position.x != previous->second.x) ||
                      (position.y != previous->second.y)))
        {
            moved = true;
        }

        node.position = position;
        current_view.node_positions[id] = position;
    }

    // Placement decides the execution order of the siblings, so moving a node
    // is an edition of the tree, not just of its picture.
    if (moved && is_edit_mode)
    {
        m_is_modified = true;
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
void Editor::collectBranch(ID p_node_id, std::unordered_set<ID>& p_branch)
{
    Node* node = findNode(p_node_id);
    if (node == nullptr)
        return;

    if (!p_branch.insert(p_node_id).second)
        return;

    // What a SubTree node runs is its definition, edited and stored in its own
    // view: the branch stops here.
    if (node->type == "SubTree")
        return;

    for (ID child_id : node->children)
    {
        collectBranch(child_id, p_branch);
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

    // A SubTree node is a call site: give it the definition it calls right
    // away, otherwise there is no tab to open and nowhere to put its children.
    if (p_type == "SubTree")
    {
        std::string const reference = createSubTreeDefinition(p_name);
        if (Node* node_ptr = findNode(id); node_ptr != nullptr)
        {
            node_ptr->subtree_reference = reference;
        }
    }

    // Adopt the node as the root while the view has none, so that the very
    // first node of a document, or the first one after the root was deleted,
    // gives the tree something to hang from.
    TreeView& current_view = getCurrentTreeView();
    if (findNode(current_view.root_id) == nullptr)
    {
        current_view.root_id = id;
    }

    m_is_modified = true;
    layoutIfAuto();

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
        if (!node->children.empty() &&
            (findTreeViewByRootId(p_node_id) != nullptr))
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

    // Any view may have held the node: the one it was the root of loses its
    // root, and all of them lose its stored position.
    for (auto& [name, view] : m_tree_views)
    {
        if (view.root_id == p_node_id)
        {
            view.root_id = promoted_root;
        }
        view.node_positions.erase(p_node_id);
    }

    m_selected_nodes.erase(p_node_id);
    if (m_selected_node_id == p_node_id)
    {
        m_selected_node_id =
            m_selected_nodes.empty() ? -1 : *m_selected_nodes.begin();
    }

    m_is_modified = true;
    layoutIfAuto();
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

    // A decorator wraps exactly one child, and a leaf none at all. Letting the
    // user attach more would build a tree the engine refuses to instantiate.
    std::string const category = nodeCategory(from->type);
    if (category == "Leaf")
        return;
    if ((category == "Decorator" || from->type == "SubTree") &&
        !from->children.empty())
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
    layoutIfAuto();

    onLinkCreated.emit(p_from_node, p_to_node);
}

// ----------------------------------------------------------------------------
void Editor::setAutoLayoutEnabled(bool const p_enabled)
{
    m_auto_layout = p_enabled;
    if (m_auto_layout)
    {
        autoLayoutNodes();
    }
}

// ----------------------------------------------------------------------------
void Editor::layoutIfAuto()
{
    if (m_auto_layout && m_has_document)
    {
        autoLayoutNodes();
    }
}

// ----------------------------------------------------------------------------
void Editor::selectNode(ID const p_node_id)
{
    m_selected_nodes.clear();
    m_selected_node_id = p_node_id;
    if (p_node_id >= 0)
    {
        m_selected_nodes.insert(p_node_id);
    }
}

// ----------------------------------------------------------------------------
void Editor::toggleNodeSelection(ID const p_node_id)
{
    if (p_node_id < 0)
        return;

    if (m_selected_nodes.erase(p_node_id) > 0U)
    {
        m_selected_node_id =
            m_selected_nodes.empty() ? -1 : *m_selected_nodes.begin();
        return;
    }

    m_selected_nodes.insert(p_node_id);
    m_selected_node_id = p_node_id;
}

// ----------------------------------------------------------------------------
void Editor::clearSelection()
{
    m_selected_nodes.clear();
    m_selected_node_id = -1;
}

// ----------------------------------------------------------------------------
void Editor::deleteSelection()
{
    // Copied: deleteNode edits the selection as it goes.
    std::vector<ID> const doomed(m_selected_nodes.begin(),
                                 m_selected_nodes.end());
    for (ID node_id : doomed)
    {
        deleteNode(node_id);
    }
    clearSelection();
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
            layoutIfAuto();

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
        createTreeView(default_name, false, -1);
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
Editor::TreeView& Editor::createTreeView(std::string const& p_name,
                                         bool const p_is_subtree,
                                         ID const p_root_id)
{
    if (!m_blackboard)
    {
        m_blackboard = std::make_shared<bt::Blackboard>();
    }

    TreeView& view = m_tree_views[p_name];
    view.name = p_name;
    view.is_subtree = p_is_subtree;
    view.root_id = p_root_id;
    view.layout_direction = m_layout_direction;
    view.blackboard = p_is_subtree ? m_blackboard->createChild() : m_blackboard;

    return view;
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
Editor::TreeView* Editor::findTreeViewOfNode(ID p_node_id)
{
    // The view a node belongs to is the one holding the top of its branch,
    // either as its root or, for a branch not attached yet, as a position.
    ID const topmost = topmostAncestor(p_node_id);
    if (TreeView* view = findTreeViewByRootId(topmost); view != nullptr)
    {
        return view;
    }

    for (auto& [name, view] : m_tree_views)
    {
        if (view.node_positions.count(topmost) > 0U)
        {
            return &view;
        }
    }

    return nullptr;
}

// ----------------------------------------------------------------------------
std::string Editor::uniqueTreeViewName(std::string const& p_base) const
{
    std::string const base = p_base.empty() ? std::string("SubTree") : p_base;
    if (m_tree_views.find(base) == m_tree_views.end())
    {
        return base;
    }

    for (int suffix = 2; suffix < 10000; ++suffix)
    {
        std::string const candidate = base + "_" + std::to_string(suffix);
        if (m_tree_views.find(candidate) == m_tree_views.end())
        {
            return candidate;
        }
    }

    return base;
}

// ----------------------------------------------------------------------------
std::size_t Editor::countSubTreeReferences(std::string const& p_name,
                                           ID const p_ignored) const
{
    std::size_t count = 0U;
    for (auto const& [id, node] : m_nodes)
    {
        if ((id != p_ignored) && (node.type == "SubTree") &&
            (node.subtree_reference == p_name))
        {
            ++count;
        }
    }
    return count;
}

// ----------------------------------------------------------------------------
void Editor::reorderChildrenByPosition(TreeView const& p_view)
{
    bool const top_to_bottom =
        (p_view.layout_direction == LayoutDirection::TopToBottom);

    auto breadthOf = [&p_view, top_to_bottom](ID p_id) {
        auto const it = p_view.node_positions.find(p_id);
        if (it == p_view.node_positions.end())
        {
            return 0.0f;
        }
        return top_to_bottom ? it->second.x : it->second.y;
    };

    for (auto& [id, node] : m_nodes)
    {
        if (node.children.size() < 2U)
            continue;

        // Only the nodes this view draws: another view knows nothing of where
        // the user dropped them.
        if (p_view.node_positions.find(id) == p_view.node_positions.end())
            continue;

        // Stable so that two children sharing a coordinate keep the order they
        // were attached in, rather than swapping at every frame.
        std::stable_sort(node.children.begin(),
                         node.children.end(),
                         [&breadthOf](ID p_lhs, ID p_rhs) {
                             return breadthOf(p_lhs) < breadthOf(p_rhs);
                         });
    }
}

// ----------------------------------------------------------------------------
void Editor::reorderChildrenByPosition()
{
    for (auto const& [name, view] : m_tree_views)
    {
        reorderChildrenByPosition(view);
    }
}

// ----------------------------------------------------------------------------
void Editor::autoLayoutNodes()
{
    std::vector<ID> const roots = collectViewRoots();
    if (roots.empty())
        return;

    // Read the placement the user gave the nodes before overwriting it: this is
    // what decides the execution order of the siblings, as in Groot.
    reorderChildrenByPosition(getCurrentTreeView());

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
void Editor::refreshActiveSubTreeScope()
{
    TreeView& view = getCurrentTreeView();
    if (!view.is_subtree || !m_blackboard)
        return;

    if (!view.blackboard)
    {
        view.blackboard = m_blackboard->createChild();
    }

    // Everything here is derived from the remapping, so a port the user removed
    // must not linger from a previous frame.
    for (std::string const& key : view.blackboard->keys())
    {
        view.blackboard->remove(key);
    }

    // The lowest ID among the call sites, so that the preview does not jump
    // from one to the other: m_nodes is unordered.
    Node const* call = nullptr;
    for (auto const& [id, node] : m_nodes)
    {
        if ((node.type != "SubTree") || (node.subtree_reference != view.name))
            continue;
        if ((call == nullptr) || (id < call->id))
        {
            call = &node;
        }
    }

    if (call == nullptr)
        return;

    auto pushPort = [this, &view](Port const& p_port) {
        if (p_port.name.empty())
            return;

        bool const is_reference = (p_port.binding.size() > 3U) &&
                                  (p_port.binding.compare(0, 2, "${") == 0) &&
                                  (p_port.binding.back() == '}');
        if (!is_reference)
        {
            view.blackboard->set(p_port.name, p_port.binding);
            return;
        }

        std::string const key =
            p_port.binding.substr(2U, p_port.binding.size() - 3U);
        if (auto raw = m_blackboard->raw(key); raw.has_value())
        {
            view.blackboard->setRaw(p_port.name, *raw);
            return;
        }

        // An output: the parent key does not exist yet, the subtree is the one
        // that will write it. Only the name is known here.
        view.blackboard->setRaw(p_port.name, bt::Blackboard::Value{});
    };

    for (Port const& port : call->inputs)
    {
        pushPort(port);
    }
    for (Port const& port : call->outputs)
    {
        pushPort(port);
    }
}

// ----------------------------------------------------------------------------
std::string Editor::createSubTreeDefinition(std::string const& p_name)
{
    std::string const name = uniqueTreeViewName(p_name);
    createTreeView(name, true, -1);
    m_is_modified = true;
    return name;
}

// ----------------------------------------------------------------------------
void Editor::moveBranchToView(ID p_node_id,
                              TreeView& p_from,
                              TreeView& p_to,
                              ImVec2 const p_offset)
{
    if (&p_from == &p_to)
        return;

    std::unordered_set<ID> branch;
    collectBranch(p_node_id, branch);

    for (ID id : branch)
    {
        ImVec2 position(0.0f, 0.0f);
        auto const it = p_from.node_positions.find(id);
        if (it != p_from.node_positions.end())
        {
            position = it->second;
            p_from.node_positions.erase(it);
        }
        p_to.node_positions[id] =
            ImVec2(position.x + p_offset.x, position.y + p_offset.y);
    }
}

// ----------------------------------------------------------------------------
std::string Editor::convertToSubTree(ID const p_node_id)
{
    Node* node = findNode(p_node_id);
    if ((node == nullptr) || (node->type == "SubTree"))
        return "";

    TreeView* source = findTreeViewOfNode(p_node_id);
    if (source == nullptr)
        return "";

    // Remember where the branch hung: the SubTree node replacing it takes the
    // very same slot, so the execution order of its siblings is preserved.
    ID const parent_id = node->parent;
    std::size_t child_index = 0U;
    if (Node* parent = findNode(parent_id); parent != nullptr)
    {
        auto const it = std::find(
            parent->children.begin(), parent->children.end(), p_node_id);
        if (it != parent->children.end())
        {
            child_index = static_cast<std::size_t>(
                std::distance(parent->children.begin(), it));
            parent->children.erase(it);
        }
    }
    node->parent = -1;

    ImVec2 branch_origin(c_layout_origin, c_layout_origin);
    if (auto const it = source->node_positions.find(p_node_id);
        it != source->node_positions.end())
    {
        branch_origin = it->second;
    }

    std::string const definition =
        createSubTreeDefinition(node->name.empty() ? node->type : node->name);

    TreeView& target = m_tree_views[definition];
    moveBranchToView(p_node_id,
                     *source,
                     target,
                     ImVec2(c_layout_origin - branch_origin.x,
                            c_layout_origin - branch_origin.y));
    target.root_id = p_node_id;

    // The call site: a SubTree node referencing the definition just filled.
    ID const call_id = getNextNodeId();
    Node call;
    call.id = call_id;
    call.type = "SubTree";
    call.name = definition;
    call.subtree_reference = definition;
    call.parent = parent_id;
    m_nodes.emplace(call_id, std::move(call));
    source->node_positions[call_id] = branch_origin;

    if (Node* parent = findNode(parent_id); parent != nullptr)
    {
        child_index = std::min(child_index, parent->children.size());
        parent->children.insert(parent->children.begin() +
                                    static_cast<std::ptrdiff_t>(child_index),
                                call_id);
    }
    else if (source->root_id == p_node_id)
    {
        // The whole tree was extracted: it now hangs from the call site.
        source->root_id = call_id;
    }

    selectNode(call_id);
    m_is_modified = true;
    layoutIfAuto();

    return definition;
}

// ----------------------------------------------------------------------------
bool Editor::inlineSubTree(ID const p_node_id)
{
    Node* node = findNode(p_node_id);
    if ((node == nullptr) || (node->type != "SubTree"))
        return false;

    std::string const reference = node->subtree_reference;
    auto const definition_it = m_tree_views.find(reference);
    if ((definition_it == m_tree_views.end()) ||
        !definition_it->second.is_subtree)
        return false;

    ID const definition_root = definition_it->second.root_id;
    if (findNode(definition_root) == nullptr)
        return false;

    // Inlining consumes the definition: another call site would be left
    // pointing at nothing.
    if (countSubTreeReferences(reference, p_node_id) > 0U)
        return false;

    // Work on a collapsed node so that the definition root is not also a child
    // of the call site while it is moved.
    if (node->is_expanded)
    {
        collapseSubTree(node);
        node->is_expanded = false;
    }

    TreeView* target = findTreeViewOfNode(p_node_id);
    if (target == nullptr)
        return false;

    std::string const target_name = target->name;
    ID const parent_id = node->parent;
    std::size_t child_index = 0U;
    if (Node* parent = findNode(parent_id); parent != nullptr)
    {
        auto const it = std::find(
            parent->children.begin(), parent->children.end(), p_node_id);
        if (it != parent->children.end())
        {
            child_index = static_cast<std::size_t>(
                std::distance(parent->children.begin(), it));
            parent->children.erase(it);
        }
    }

    ImVec2 call_position(c_layout_origin, c_layout_origin);
    if (auto const it = target->node_positions.find(p_node_id);
        it != target->node_positions.end())
    {
        call_position = it->second;
    }

    ImVec2 definition_origin(c_layout_origin, c_layout_origin);
    if (auto const it =
            definition_it->second.node_positions.find(definition_root);
        it != definition_it->second.node_positions.end())
    {
        definition_origin = it->second;
    }

    moveBranchToView(definition_root,
                     definition_it->second,
                     *target,
                     ImVec2(call_position.x - definition_origin.x,
                            call_position.y - definition_origin.y));

    if (Node* inlined_root = findNode(definition_root); inlined_root != nullptr)
    {
        inlined_root->parent = parent_id;
    }

    if (Node* parent = findNode(parent_id); parent != nullptr)
    {
        child_index = std::min(child_index, parent->children.size());
        parent->children.insert(parent->children.begin() +
                                    static_cast<std::ptrdiff_t>(child_index),
                                definition_root);
    }
    else if (target->root_id == p_node_id)
    {
        target->root_id = definition_root;
    }

    m_nodes.erase(p_node_id);
    for (auto& [name, view] : m_tree_views)
    {
        view.node_positions.erase(p_node_id);
    }
    m_tree_views.erase(reference);
    m_active_tree_name = target_name;

    selectNode(definition_root);
    m_is_modified = true;
    layoutIfAuto();

    return true;
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
    m_selected_nodes.clear();
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
        createTreeView(tree_name, false, root_id);
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
            .forEachMap([&](std::string_view p_subtree_name,
                            bt::YamlNode p_subtree_def) {
                std::string subtree_name(p_subtree_name);
                ID subtree_root_id = parseYamlNode(p_subtree_def, -1);

                if (subtree_root_id < 0)
                {
                    std::cerr << "Failed to parse SubTree: " << subtree_name
                              << std::endl;
                    return;
                }

                createTreeView(subtree_name, true, subtree_root_id);
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
    m_selected_nodes.clear();
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
            .forEachMap([&](std::string_view p_subtree_name,
                            bt::YamlNode p_subtree_def) {
                std::string subtree_name(p_subtree_name);
                ID subtree_root_id = parseYamlNode(p_subtree_def, -1);

                if (subtree_root_id < 0)
                {
                    std::cerr << "Failed to parse SubTree: " << subtree_name
                              << std::endl;
                    return;
                }

                createTreeView(subtree_name, true, subtree_root_id);
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

        createTreeView(tree_name, false, root_id);
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
    // The file records the execution order, so it has to match what the user
    // sees: children are written left to right, as they are drawn.
    reorderChildrenByPosition();

    // The BehaviorTree section is the main tree, whichever tab happens to be
    // open: saving from a subtree tab must not promote it to main tree.
    ID root_id = -1;
    for (auto const& [name, view] : m_tree_views)
    {
        if (!view.is_subtree && (findNode(view.root_id) != nullptr))
        {
            root_id = view.root_id;
            break;
        }
    }

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
                // Indenting a blank line would leave trailing spaces behind.
                if (!line.empty())
                {
                    out << "  " << line;
                }
                out << '\n';
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

        // The views live in a hash map, so they are written in a stable order
        // rather than one the standard library is free to change: saving twice
        // must not shuffle the file under version control.
        std::vector<std::string> names;
        names.reserve(m_tree_views.size());
        for (auto const& [name, view] : m_tree_views)
        {
            if (view.is_subtree && view.root_id >= 0)
            {
                names.push_back(name);
            }
        }
        std::sort(names.begin(), names.end());

        for (auto const& name : names)
        {
            Node* subtree_root = findNode(m_tree_views.at(name).root_id);
            if (subtree_root)
            {
                out << "  " << name << ":\n";
                serializeNodeToYaml(out, subtree_root, 2, true, false);
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

    for (auto const& [key, value] : p_node->attributes)
    {
        appendYamlIndent(p_out, inner);
        p_out << key << ": ";
        appendYamlScalar(p_out, value);
        p_out << '\n';
    }

    // Both directions go into the single block bt::Builder reads. A port whose
    // binding is ${key} is wired to that blackboard key; on a SubTree node this
    // is the remapping of a key of the nested scope onto one of the parent.
    // Writing "inputs" and "outputs" instead, as this editor used to, produced
    // files the engine silently ignored.
    if (!p_node->inputs.empty() || !p_node->outputs.empty())
    {
        appendYamlIndent(p_out, inner);
        p_out << "parameters:\n";

        auto writePort = [&](Port const& p_port) {
            appendYamlIndent(p_out, inner + 1);
            p_out << p_port.name << ": ";
            appendYamlScalar(p_out,
                             p_port.binding.empty() ? ("${" + p_port.name + "}")
                                                    : p_port.binding);
            p_out << '\n';
        };

        for (auto const& input : p_node->inputs)
        {
            writePort(input);
        }
        for (auto const& output : p_node->outputs)
        {
            writePort(output);
        }
    }

    // The engine infers the direction of a port from the blackboard, so this
    // list is editor metadata only: it tells which parameters to put back on
    // the output side when the file is reopened.
    if (!p_node->outputs.empty())
    {
        appendYamlIndent(p_out, inner);
        p_out << "outputs:\n";
        for (auto const& output : p_node->outputs)
        {
            appendYamlIndent(p_out, inner + 1);
            p_out << "- ";
            appendYamlScalar(p_out, output.name);
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
        // bt::Builder reads a decorator's single child under "child" and
        // rejects the file when it finds "children" instead.
        appendYamlIndent(p_out, inner);
        p_out << (isDecoratorType(p_node->type) ? "child" : "children")
              << ":\n";
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

    // Ports keep the expression they are bound to, so that a parameter reading
    // "goal: ${target}" is not rewritten as "goal: ${goal}" on the next save.
    auto readPorts = [](bt::YamlNode const& p_block,
                        std::vector<Port>& p_ports) {
        p_block.forEachMap([&p_ports](std::string_view p_name,
                                      bt::YamlNode p_value) {
            std::string name(p_name);
            std::string binding =
                p_value.valid() ? p_value.scalar() : std::string();
            if (binding.empty())
            {
                binding = "${" + name + "}";
            }

            auto it = std::find_if(
                p_ports.begin(), p_ports.end(), [&name](Port const& p_port) {
                    return p_port.name == name;
                });
            if (it != p_ports.end())
            {
                it->binding = std::move(binding);
                return;
            }
            p_ports.push_back({std::move(name), std::move(binding)});
        });
    };

    // "inputs" only exists in files written by older versions of this editor.
    if (node_data.hasKey("inputs"))
    {
        readPorts(node_data.child("inputs"), editor_node.inputs);
    }

    if (node_data.hasKey("parameters"))
    {
        readPorts(node_data.child("parameters"), editor_node.inputs);
    }

    if (node_data.hasKey("outputs"))
    {
        bt::YamlNode const outputs = node_data.child("outputs");
        if (outputs.isSeq())
        {
            // A list of names picking, among the parameters, those the node
            // writes rather than reads.
            outputs.forEachSeq([&](bt::YamlNode p_entry) {
                std::string const name = p_entry.scalar();
                auto it = std::find_if(editor_node.inputs.begin(),
                                       editor_node.inputs.end(),
                                       [&name](Port const& p_port) {
                                           return p_port.name == name;
                                       });
                if (it != editor_node.inputs.end())
                {
                    editor_node.outputs.push_back(*it);
                    editor_node.inputs.erase(it);
                }
                else if (!name.empty())
                {
                    editor_node.outputs.push_back({name, "${" + name + "}"});
                }
            });
        }
        else
        {
            readPorts(outputs, editor_node.outputs);
        }
    }

    static std::array<std::string_view, 7> const known_keys = {"name",
                                                               "reference",
                                                               "inputs",
                                                               "outputs",
                                                               "parameters",
                                                               "children",
                                                               "child"};
    node_data.forEachMap(
        [&editor_node](std::string_view p_key, bt::YamlNode p_value) {
            if (std::find(known_keys.begin(), known_keys.end(), p_key) !=
                known_keys.end())
            {
                return;
            }
            // Maps and sequences have no faithful flat representation here, so
            // they are left out rather than written back mangled.
            if (!p_value.valid() || p_value.isMap() || p_value.isSeq())
                return;
            editor_node.attributes.emplace_back(std::string(p_key),
                                                p_value.scalar());
        });

    if (node_data.hasKey("children"))
    {
        bt::YamlNode children = node_data.child("children");
        if (children.isSeq())
        {
            children.forEachSeq([&](bt::YamlNode p_child) {
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
