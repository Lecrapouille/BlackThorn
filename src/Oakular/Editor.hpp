/**
 * @file Editor.hpp
 * @brief Oakular - Embeddable behavior tree editor widget.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/BlackThorn
 */

#pragma once

#include "BlackThorn/BlackThorn.hpp"
#include "BlackThorn/Common/Signal.hpp"

#include <imgui.h>

#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace bt {
class YamlNode;
}

namespace oakular {

class TreeRenderer;
class Server;

// ****************************************************************************
//! \brief Embeddable behavior tree editor.
//!
//! Provides graphical edition and real-time visualization of behavior trees:
//! - Tree management (nodes, links, subtrees)
//! - YAML serialization/deserialization
//! - Auto-layout
//! - Node palette, node edition popup and blackboard panel
//!
//! This class is a pure Dear ImGui component: it never creates a window, an
//! OpenGL context or an ImGui context, and never calls GLFW. The host owns the
//! frame and calls, between \c ImGui::NewFrame() and \c ImGui::Render():
//! \code
//!   oakular::Editor editor;
//!
//!   // once per frame
//!   editor.update(delta_time);
//!   editor.draw("Behavior Tree");
//!
//!   // from inside the host menu bar
//!   editor.drawMenuBar();
//! \endcode
//!
//! Actions the editor cannot perform on its own (picking a file, closing the
//! application) are exposed as signals the host connects to.
// ****************************************************************************
class Editor
{
    friend class TreeRenderer;

public:

    //! \brief Type alias for node and link IDs
    using ID = int32_t;

    // ------------------------------------------------------------------------
    //! \brief Editor mode: Editor or Visualizer.
    //! - Creation: Create and edit behavior trees graphically.
    //! - Visualizer: Display runtime behavior trees from TCP clients.
    // ------------------------------------------------------------------------
    enum class Mode
    {
        Creation,  //!< Mode for creating trees graphically
        Visualizer //!< Mode for displaying in real-time a tree connected to a
                   //!< client
    };

    // ------------------------------------------------------------------------
    //! \brief Layout direction for displaying the tree.
    // ------------------------------------------------------------------------
    enum class LayoutDirection
    {
        LeftToRight = 0, //!< Layout direction from left to right
        TopToBottom = 1  //!< Layout direction from top to bottom
    };

    // ------------------------------------------------------------------------
    //! \brief Kind of file dialog the editor asks the host to open.
    //! \see Editor::onFileDialogRequested
    // ------------------------------------------------------------------------
    enum class FileDialog
    {
        Load, //!< The host shall pick an existing YAML file to load
        Save  //!< The host shall pick a destination YAML file to save to
    };

    // ------------------------------------------------------------------------
    //! \brief Link between nodes.
    // ------------------------------------------------------------------------
    struct Link
    {
        //! \brief Link ID
        ID id = 0;
        //! \brief From node ID
        ID from_node = 0;
        //! \brief To node ID
        ID to_node = 0;
    };

    // ------------------------------------------------------------------------
    //! \brief A blackboard port of a node, as written in the \c parameters
    //! block of the YAML file.
    //!
    //! \c binding is what the engine resolves: either \c ${key} to read or
    //! write the blackboard entry \c key, or a literal value. On a SubTree node
    //! the port is the remapping of a key of the child scope onto a key of the
    //! parent one.
    // ------------------------------------------------------------------------
    struct Port
    {
        //! \brief Name of the port, as the node code asks for it.
        std::string name;
        //! \brief Blackboard reference \c ${key} or literal value.
        std::string binding;
    };

    // ------------------------------------------------------------------------
    //! \brief Graphical representation of a behavior tree node.
    // ------------------------------------------------------------------------
    struct Node
    {
        //! \brief Node ID
        ID id;
        //! \brief Node type ("Sequence", "Selector", etc.)
        std::string type;
        //! \brief User-defined name
        std::string name;
        //! \brief Node position
        ImVec2 position = ImVec2(0, 0);
        //! \brief Node children
        std::vector<ID> children;
        //! \brief Node parent
        ID parent = -1;
        //! \brief Blackboard input ports, read by the node.
        std::vector<Port> inputs;
        //! \brief Blackboard output ports, written by the node.
        std::vector<Port> outputs;
        //! \brief Scalar keys read from the file that the editor does not
        //! model, such as the \c _id of a node or the \c key and \c value of a
        //! SetBlackboard. Written back verbatim so that opening a tree and
        //! saving it does not quietly strip what makes it work.
        std::vector<std::pair<std::string, std::string>> attributes;
        //! \brief SubTree reference (for SubTree nodes)
        std::string subtree_reference;
        //! \brief SubTree expansion state
        bool is_expanded = false;
        //! \brief Optional: reference to actual BT node
        std::shared_ptr<bt::Node> bt_node;
        //! \brief Runtime status for visualizer mode (0=INVALID, 1=RUNNING,
        //! 2=SUCCESS, 3=FAILURE)
        int runtime_status = 0;
    };

    // ------------------------------------------------------------------------
    //! \brief Tree view for tab management
    // ------------------------------------------------------------------------
    struct TreeView
    {
        //! \brief Name of the tree view
        std::string name;
        //! \brief If the tree view is a subtree
        bool is_subtree;
        //! \brief Root node ID of the tree view
        ID root_id;
        //! \brief Layout direction for displaying the tree
        LayoutDirection layout_direction = LayoutDirection::TopToBottom;
        //! \brief Stored node positions for this view (node_id -> position)
        std::unordered_map<ID, ImVec2> node_positions;
        //! \brief Blackboard scope of the view. The main tree owns the root
        //! blackboard; a subtree owns a child of it, exactly like the nested
        //! blackboard \c bt::Builder creates at runtime, so that a key not
        //! defined locally is read from the parent scope.
        bt::Blackboard::Ptr blackboard;
    };

    // ------------------------------------------------------------------------
    //! \brief A node type offered by the creation palette.
    //! Built-in types are registered by the constructor. A host registers its
    //! own domain nodes with \c registerNodeType.
    // ------------------------------------------------------------------------
    struct NodeType
    {
        //! \brief Name used both in the palette and as the YAML node type
        std::string name;
        //! \brief Palette grouping, e.g. "Composite", "Leaf" or "Custom"
        std::string category;
        //! \brief Whether the node may declare blackboard input/output ports
        bool can_have_ports = false;
    };

    // ------------------------------------------------------------------------
    //! \brief Default constructor. Creates an empty tree with its own
    //! blackboard.
    // ------------------------------------------------------------------------
    Editor();

    // ------------------------------------------------------------------------
    //! \brief Constructor sharing a blackboard owned by the host.
    //! \param p_blackboard The blackboard to edit. A fresh one is created when
    //! null.
    // ------------------------------------------------------------------------
    explicit Editor(bt::Blackboard::Ptr p_blackboard);

    // ------------------------------------------------------------------------
    //! \brief Destructor.
    // ------------------------------------------------------------------------
    virtual ~Editor();

    Editor(Editor const&) = delete;
    Editor& operator=(Editor const&) = delete;

    // ========================================================================
    // Life cycle. Driven by the host.
    // ========================================================================

    // ------------------------------------------------------------------------
    //! \brief Re-initialize the editor: fresh renderer and empty tree.
    //! Already called by the constructor, so a host only needs it to restart
    //! the editor from scratch.
    //! \return true on success.
    // ------------------------------------------------------------------------
    bool setup();

    // ------------------------------------------------------------------------
    //! \brief Release the renderer and detach the visualizer server.
    //! Called by the destructor.
    // ------------------------------------------------------------------------
    void teardown();

    // ------------------------------------------------------------------------
    //! \brief Advance the editor logic. Call once per frame, outside of the
    //! ImGui draw calls. In visualizer mode this polls the attached server and
    //! loads incoming trees.
    //! \param p_dt Delta time in seconds since the previous frame.
    // ------------------------------------------------------------------------
    void update(float p_dt);

    // ========================================================================
    // Rendering. All these methods must be called inside an ImGui frame.
    // ========================================================================

    // ------------------------------------------------------------------------
    //! \brief Draw the whole editor: the tree window, the blackboard panel when
    //! visible, and the keyboard shortcuts.
    //! \param p_title Title of the ImGui window holding the tree.
    // ------------------------------------------------------------------------
    void draw(char const* p_title = "Behavior Tree");

    // ------------------------------------------------------------------------
    //! \brief Draw only the ImGui window holding the tree canvas.
    //! \param p_title Title of the ImGui window.
    // ------------------------------------------------------------------------
    void drawEditorWindow(char const* p_title);

    // ------------------------------------------------------------------------
    //! \brief Draw only the blackboard panel, in its own ImGui window.
    //! Does nothing when the panel is hidden.
    // ------------------------------------------------------------------------
    void drawBlackboardPanel();

    // ------------------------------------------------------------------------
    //! \brief Draw the editor menus (File, Edit, View, Mode).
    //! Call this from within the host menu bar, between \c ImGui::BeginMenuBar
    //! and \c ImGui::EndMenuBar.
    // ------------------------------------------------------------------------
    void drawMenuBar();

    // ------------------------------------------------------------------------
    //! \brief Draw the tree canvas alone, at the current ImGui cursor.
    // ------------------------------------------------------------------------
    void drawBehaviorTree();

    // ------------------------------------------------------------------------
    //! \brief Handle the editor keyboard shortcuts (Ctrl+N, Ctrl+O, Ctrl+S,
    //! Ctrl+Shift+S, Ctrl+L, Ctrl+Q). Already called by \c draw.
    // ------------------------------------------------------------------------
    void handleKeyboardShortcuts();

    // ========================================================================
    // Tree edition.
    // ========================================================================

    // ------------------------------------------------------------------------
    //! \brief Reset the editor to an empty tree with a fresh blackboard and no
    //! open document. Edition stays disabled until a document is opened.
    // ------------------------------------------------------------------------
    void reset();

    // ------------------------------------------------------------------------
    //! \brief Start a new, empty and unnamed document. This is what enables
    //! edition: without a document there is nothing to add a node to.
    // ------------------------------------------------------------------------
    void newDocument();

    // ------------------------------------------------------------------------
    //! \brief Check whether a document is open, either created by
    //! \c newDocument or loaded by \c loadFromYaml.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool hasDocument() const
    {
        return m_has_document;
    }

    // ------------------------------------------------------------------------
    //! \brief Check whether the tree may be edited: an open document, in
    //! creation mode. Visualizer trees are read-only.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isEditable() const
    {
        return m_has_document && (m_mode == Mode::Creation);
    }

    // ------------------------------------------------------------------------
    //! \brief Set the editor mode: Editor or Real-Time Visualizer.
    //! \param p_mode The new editor mode.
    // ------------------------------------------------------------------------
    void setMode(Mode const p_mode);

    // ------------------------------------------------------------------------
    //! \brief Get the current editor mode.
    // ------------------------------------------------------------------------
    [[nodiscard]] Mode mode() const
    {
        return m_mode;
    }

    // ------------------------------------------------------------------------
    //! \brief Node operations: add a new node to the tree.
    //! \param p_type The type of the node (e.g. "Action", "Condition",
    //! "Composite", "Decorator").
    //! \param p_name The name of the node.
    //! \return The ID of the new node.
    // ------------------------------------------------------------------------
    ID addNode(std::string const& p_type, std::string const& p_name);

    // ------------------------------------------------------------------------
    //! \brief Add a node and link it to the pending link from node if any.
    //! \param p_type The type of the node.
    //! \param p_name The name of the node.
    // ------------------------------------------------------------------------
    void addNodeAndLink(std::string const& p_type, std::string const& p_name);

    // ------------------------------------------------------------------------
    //! \brief Delete a node from the tree.
    //! \param p_node_id The ID of the node to delete.
    // ------------------------------------------------------------------------
    void deleteNode(ID const p_node_id);

    // ------------------------------------------------------------------------
    //! \brief Create a link between two nodes.
    //! \param p_from_node The source node ID.
    //! \param p_to_node The target node ID.
    // ------------------------------------------------------------------------
    void createLink(ID const p_from_node, ID const p_to_node);

    // ------------------------------------------------------------------------
    //! \brief Delete a link between two nodes.
    //! \param p_from_node The source node ID.
    //! \param p_to_node The target node ID.
    // ------------------------------------------------------------------------
    void deleteLink(ID const p_from_node, ID const p_to_node);

    // ------------------------------------------------------------------------
    //! \brief Load a tree from a YAML file. On success the file becomes the
    //! open document.
    //! \param p_filepath The path to the YAML file.
    //! \return true when the tree was loaded.
    // ------------------------------------------------------------------------
    bool loadFromYaml(std::string const& p_filepath);

    // ------------------------------------------------------------------------
    //! \brief Load a tree from a YAML string (for visualizer mode).
    //! \param p_yaml_content The YAML content as a string.
    //! \return true when the tree was loaded.
    // ------------------------------------------------------------------------
    bool loadFromYamlString(std::string const& p_yaml_content);

    // ------------------------------------------------------------------------
    //! \brief Save a tree to a YAML file, which becomes the open document.
    //! \param p_filepath The path to the YAML file.
    //! \return true when the tree reached the disk.
    // ------------------------------------------------------------------------
    bool saveToYaml(std::string const& p_filepath);

    // ------------------------------------------------------------------------
    //! \brief Save the tree back to the file it came from. When the document
    //! has no file yet, ask the host for a save dialog through
    //! \c onFileDialogRequested instead.
    //! \return true when the tree reached the disk, false when a dialog was
    //! requested or when the write failed.
    // ------------------------------------------------------------------------
    bool save();

    // ------------------------------------------------------------------------
    //! \brief Auto-layout the nodes in the tree.
    // ------------------------------------------------------------------------
    void autoLayoutNodes();

    // ------------------------------------------------------------------------
    //! \brief Check whether the layout is recomputed after each structural
    //! change instead of on explicit request only.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isAutoLayoutEnabled() const
    {
        return m_auto_layout;
    }

    // ------------------------------------------------------------------------
    //! \brief Recompute the layout after each structural change, or leave the
    //! nodes where the user dropped them.
    //! \param p_enabled Whether to follow the structure.
    // ------------------------------------------------------------------------
    void setAutoLayoutEnabled(bool const p_enabled);

    // ------------------------------------------------------------------------
    //! \brief Reorder the children of every node after the position the user
    //! gave them, left to right in a top-to-bottom layout, top to bottom in a
    //! left-to-right one. This is what makes the drawing, and not the order the
    //! links happened to be created in, decide the execution order.
    // ------------------------------------------------------------------------
    void reorderChildrenByPosition();

    // ------------------------------------------------------------------------
    //! \brief Toggle the expansion state of a SubTree node.
    //! \param p_node_id The ID of the SubTree node.
    // ------------------------------------------------------------------------
    void toggleSubTreeExpansion(ID const p_node_id);

    // ========================================================================
    // SubTrees.
    // ========================================================================

    // ------------------------------------------------------------------------
    //! \brief Create an empty SubTree definition, reachable as its own tab and
    //! owning a blackboard scope nested in the one of the main tree.
    //! \param p_name Wished name. A suffix is appended when already taken.
    //! \return The name the definition was created under.
    // ------------------------------------------------------------------------
    std::string createSubTreeDefinition(std::string const& p_name);

    // ------------------------------------------------------------------------
    //! \brief Move a node and its descendants into a new SubTree definition,
    //! leaving a SubTree node referencing it in their place.
    //! \param p_node_id Root of the extracted branch.
    //! \return Name of the created definition, empty on failure.
    // ------------------------------------------------------------------------
    std::string convertToSubTree(ID const p_node_id);

    // ------------------------------------------------------------------------
    //! \brief Inverse of \c convertToSubTree: replace a SubTree node by the
    //! nodes of its definition. The definition is dropped when no other node
    //! references it.
    //! \param p_node_id The SubTree node to inline.
    //! \return true when the node was replaced.
    // ------------------------------------------------------------------------
    bool inlineSubTree(ID const p_node_id);

    // ========================================================================
    // Selection.
    // ========================================================================

    // ------------------------------------------------------------------------
    //! \brief Nodes currently selected. Holds \c selectedNode when not empty.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::unordered_set<ID> const& selectedNodes() const
    {
        return m_selected_nodes;
    }

    // ------------------------------------------------------------------------
    //! \brief Replace the selection by a single node, or clear it with -1.
    //! \param p_node_id The node to select.
    // ------------------------------------------------------------------------
    void selectNode(ID const p_node_id);

    // ------------------------------------------------------------------------
    //! \brief Add a node to the selection, or remove it when already in.
    //! \param p_node_id The node to toggle.
    // ------------------------------------------------------------------------
    void toggleNodeSelection(ID const p_node_id);

    // ------------------------------------------------------------------------
    //! \brief Empty the selection.
    // ------------------------------------------------------------------------
    void clearSelection();

    // ------------------------------------------------------------------------
    //! \brief Delete every selected node and its descendants.
    // ------------------------------------------------------------------------
    void deleteSelection();

    // ========================================================================
    // Node palette.
    // ========================================================================

    // ------------------------------------------------------------------------
    //! \brief Add a node type to the creation palette and to the node edition
    //! popup. Use it to expose the domain nodes registered in your
    //! \c bt::NodeFactory so that they can be placed graphically.
    //! \param p_name Name of the node type, as written in the YAML file.
    //! \param p_category Palette grouping.
    //! \param p_can_have_ports Whether the node may declare blackboard ports.
    // ------------------------------------------------------------------------
    void registerNodeType(std::string const& p_name,
                          std::string const& p_category = "Custom",
                          bool const p_can_have_ports = true);

    // ------------------------------------------------------------------------
    //! \brief Get the node types offered by the palette.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::vector<NodeType> const& nodeTypes() const
    {
        return m_node_types;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the palette category of a node type.
    //! \param p_type Node type name.
    //! \return The category, empty when the type is not registered.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::string nodeCategory(std::string const& p_type) const;

    // ------------------------------------------------------------------------
    //! \brief Check whether a node type accepts a single child. Such nodes are
    //! written with the \c child key the engine expects instead of
    //! \c children.
    //! \param p_type Node type name.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isDecoratorType(std::string const& p_type) const;

    // ========================================================================
    // State exposed to the host.
    // ========================================================================

    // ------------------------------------------------------------------------
    //! \brief Check whether the tree has unsaved modifications.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isModified() const
    {
        return m_is_modified;
    }

    // ------------------------------------------------------------------------
    //! \brief Path of the currently opened behavior tree, empty if none.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::string const& filepath() const
    {
        return m_behavior_tree_filepath;
    }

    // ------------------------------------------------------------------------
    //! \brief Name of the open document as shown to the user: the file name,
    //! "Untitled" for a document never saved, empty when none is open. A
    //! trailing star marks unsaved changes.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::string documentTitle() const;

    // ------------------------------------------------------------------------
    //! \brief Get the edited blackboard, shareable with a running tree. This is
    //! the root scope: the one of the main tree.
    // ------------------------------------------------------------------------
    [[nodiscard]] bt::Blackboard::Ptr blackboard() const
    {
        return m_blackboard;
    }

    // ------------------------------------------------------------------------
    //! \brief Blackboard scope of the tab being edited. A subtree tab owns a
    //! child of the root scope: what it does not define is read from its
    //! parent, as at runtime.
    // ------------------------------------------------------------------------
    [[nodiscard]] bt::Blackboard::Ptr activeBlackboard();

    // ------------------------------------------------------------------------
    //! \brief Replace the edited blackboard by one owned by the host.
    //! \param p_blackboard The blackboard to edit. Ignored when null.
    // ------------------------------------------------------------------------
    void setBlackboard(bt::Blackboard::Ptr p_blackboard);

    // ------------------------------------------------------------------------
    //! \brief ID of the selected node, -1 when none.
    // ------------------------------------------------------------------------
    [[nodiscard]] ID selectedNode() const
    {
        return m_selected_node_id;
    }

    // ------------------------------------------------------------------------
    //! \brief Check whether the blackboard panel is visible.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isBlackboardPanelVisible() const
    {
        return m_show_blackboard_panel;
    }

    // ------------------------------------------------------------------------
    //! \brief Show or hide the blackboard panel.
    //! \param p_visible The new visibility.
    // ------------------------------------------------------------------------
    void setBlackboardPanelVisible(bool const p_visible)
    {
        m_show_blackboard_panel = p_visible;
    }

#if defined(OAKULAR_HAS_SERVER)

    // ------------------------------------------------------------------------
    //! \brief Attach the TCP server feeding the visualizer mode. Without a
    //! server the visualizer mode only reports that it is unavailable.
    //! \param p_server The server to use, shared with the host.
    // ------------------------------------------------------------------------
    void attachServer(std::shared_ptr<Server> p_server);

    // ------------------------------------------------------------------------
    //! \brief Get the attached visualizer server, nullptr when none.
    // ------------------------------------------------------------------------
    [[nodiscard]] Server* server() const
    {
        return m_server.get();
    }

#endif // OAKULAR_HAS_SERVER

    // ========================================================================
    // Signals.
    // ========================================================================

    //! \brief Emitted when a node is modified.
    //! \param p_node_id The ID of the node that was modified.
    bt::Signal<ID> onNodeModified;
    //! \brief Emitted when a link is created.
    //! \param p_from_node_id The ID of the from node.
    //! \param p_to_node_id The ID of the to node.
    bt::Signal<ID, ID> onLinkCreated;
    //! \brief Emitted when a link is deleted.
    //! \param p_link_id The ID of the link that was deleted.
    bt::Signal<ID> onLinkDeleted;
    //! \brief Emitted when the user asks to load or save a tree. The host shall
    //! open its own file browser then call \c loadFromYaml or \c saveToYaml.
    bt::Signal<FileDialog> onFileDialogRequested;
    //! \brief Emitted when the user asks to close the editor. The host decides
    //! what to do, checking \c isModified beforehand if relevant.
    bt::Signal<> onQuitRequested;

protected: // Widgets

    //! \brief Draw the tab bar holding the main tree and its subtrees.
    void showEditorTabs();

    //! \brief Draw a single tree tab.
    //! \param p_name The name of the tab.
    //! \param p_view The tree view to draw.
    void drawTreeTab(std::string const& p_name, TreeView& p_view);

    //! \brief Draw the palette popup for adding new nodes.
    void showAddNodePalette();

    //! \brief Draw the placeholder shown when no document is open.
    void showNoDocumentPanel();

    //! \brief Draw the confirmation asked before a new document discards the
    //! unsaved changes of the current one.
    void showNewDocumentConfirmation();

    //! \brief Draw the context menu for node operations.
    void showNodeContextMenu();

    //! \brief Draw the modal popup for editing node properties.
    void showNodeEditPopup();

    //! \brief Draw the line telling which document is open and whether it holds
    //! unsaved changes.
    void showDocumentStatusBar();

    //! \brief List, read-only, the keys a nested scope reads from the scope of
    //! the main tree.
    void showInheritedKeys();

    //! \brief Fill the blackboard scope of the open subtree tab from the port
    //! remapping of the SubTree node calling it, the way \c bt::Builder fills
    //! the nested blackboard at build time. The result is a preview: the scope
    //! holds nothing of its own in the file.
    void refreshActiveSubTreeScope();

    //! \brief Draw the port table of the node edition popup.
    //! \param p_label Section title.
    //! \param p_ports Ports being edited.
    //! \param p_new_name Buffer holding the name of the port being added.
    //! \param p_is_remapping Whether the ports remap a SubTree onto its parent
    //! scope, which changes the wording and the proposed keys.
    void showPortTable(char const* p_label,
                       std::vector<Port>& p_ports,
                       std::string& p_new_name,
                       bool const p_is_remapping);

    //! \brief Draw the attribute table of the node edition popup, holding the
    //! keys the editor does not interpret but the engine needs, such as the
    //! \c key and \c value of a SetBlackboard or the \c times of a Repeat.
    //! \param p_attributes Attributes being edited.
    //! \param p_new_name Buffer holding the name of the attribute being added.
    void showAttributeTable(
        std::vector<std::pair<std::string, std::string>>& p_attributes,
        std::string& p_new_name);

    //! \brief Tell the renderer which node types accept children, so that the
    //! output pin follows the palette instead of a list frozen in the renderer.
    void publishChildPolicy();

    //! \brief Draw the visualizer status line (TCP server state).
    void showVisualizerPanel();

    //! \brief Handle edit mode interactions (node selection, link creation).
    void handleEditModeInteractions();

protected: // TreeView helpers

    //! \brief Get the current tree view (creates one if needed).
    TreeView& getCurrentTreeView();

    //! \brief Create a tree view, giving it its blackboard scope.
    //! \param p_name Name of the view, assumed free.
    //! \param p_is_subtree Whether the view holds a SubTree definition.
    //! \param p_root_id Root node, -1 when the view is still empty.
    TreeView& createTreeView(std::string const& p_name,
                             bool const p_is_subtree,
                             ID const p_root_id);

    //! \brief Find a tree view by its root ID.
    TreeView* findTreeViewByRootId(ID p_root_id);

    //! \brief Find the view a node belongs to, walking up its parents.
    TreeView* findTreeViewOfNode(ID p_node_id);

    //! \brief Find a node by its ID.
    Node* findNode(ID const p_id);

    //! \brief Get the position of a node in the current view.
    ImVec2 getNodePosition(ID p_node_id);

    //! \brief Set the position of a node in the current view.
    void setNodePosition(ID p_node_id, ImVec2 p_position);

    //! \brief Collect all visible nodes from a root (handles SubTree
    //! expansion).
    void collectVisibleNodes(ID p_root_id,
                             std::unordered_set<ID>& p_visible_nodes);

    //! \brief Collect a node and all its descendants, stopping at SubTree nodes
    //! whose content belongs to another view.
    void collectBranch(ID p_node_id, std::unordered_set<ID>& p_branch);

    //! \brief Nodes acting as a root in the current view: the root of the view
    //! plus, in the main tree, the nodes not attached to a parent yet. Both the
    //! canvas and the auto-layout walk exactly these.
    [[nodiscard]] std::vector<ID> collectViewRoots();

    //! \brief Check whether a node type may declare blackboard ports.
    [[nodiscard]] bool canHaveBlackboardPorts(std::string const& p_type) const;

private: // SubTree management (internal)

    bool expandSubTree(Node* p_subtree_node);
    bool collapseSubTree(Node* p_subtree_node);

    //! \brief Build a view name not taken yet, from a wished base name.
    [[nodiscard]] std::string
    uniqueTreeViewName(std::string const& p_base) const;

    //! \brief Count the SubTree nodes referencing a definition.
    [[nodiscard]] std::size_t countSubTreeReferences(std::string const& p_name,
                                                     ID const p_ignored) const;

    //! \brief Move a node and its descendants to another view, transferring
    //! their stored positions.
    void moveBranchToView(ID p_node_id,
                          TreeView& p_from,
                          TreeView& p_to,
                          ImVec2 const p_offset);

private: // Tree conversion (internal)

    void buildTreeFromNodes();
    void buildNodesFromTree(bt::Node& p_root);
    ID buildNodesFromTreeRecursive(bt::Node& p_node, ID p_parent_id);
    void serializeNodeToYaml(std::ostringstream& p_out,
                             Node* p_node,
                             int p_indent,
                             bool p_is_subtree_definition = false,
                             bool p_sequence_item = false);
    ID parseYamlNode(bt::YamlNode const& p_yaml_node, ID p_parent_id);

private: // Auto-layout (internal)

    //! \brief Size a node occupies on the canvas, as the renderer draws it.
    [[nodiscard]] ImVec2 nodeSize(Node const& p_node) const;

    //! \brief Room a subtree needs along the axis siblings spread on: the
    //! width in top-to-bottom layout, the height in left-to-right layout.
    //! \param p_node_id Root of the measured subtree.
    //! \param p_breadths Memoized results, also breaking accidental cycles.
    float measureSubtree(ID p_node_id,
                         std::unordered_map<ID, float>& p_breadths);

    //! \brief Place a subtree, each parent centered over its children.
    //! \param p_node_id Root of the placed subtree.
    //! \param p_breadth Start of the slot reserved for the subtree.
    //! \param p_depth Coordinate of the level along the parent-to-child axis.
    //! \param p_breadths Result of \c measureSubtree.
    //! \param p_placed Nodes already placed, guarding against cycles.
    void placeSubtree(ID p_node_id,
                      float p_breadth,
                      float p_depth,
                      std::unordered_map<ID, float> const& p_breadths,
                      std::unordered_set<ID>& p_placed);

    //! \brief Copy the positions of the current view back into the nodes, so
    //! that a caller not going through the canvas sees them too.
    void syncNodePositionsFromView();

    //! \brief Reorder the children of the nodes of a view after their position.
    void reorderChildrenByPosition(TreeView const& p_view);

    //! \brief Recompute the layout, but only when the user asked the editor to
    //! keep it in sync with the structure. Called after every edition.
    void layoutIfAuto();

private: // Tree structure helpers (internal)

    //! \brief Check whether \p p_candidate is an ancestor of \p p_node.
    [[nodiscard]] bool isAncestorOf(ID p_candidate, ID p_node);

    //! \brief Walk up the parents of \p p_node up to the topmost one.
    [[nodiscard]] ID topmostAncestor(ID p_node);

    void registerBuiltinNodeTypes();

private: // auto-increment unique identifiers

    inline ID getNextNodeId()
    {
        return m_unique_node_id++;
    }

protected:

    //! \brief Mode: visualizer or editor.
    Mode m_mode = Mode::Creation;
    //! \brief Indicate how to render trees: from left to right or top to
    //! bottom.
    LayoutDirection m_layout_direction = LayoutDirection::TopToBottom;
    //! \brief Renderer for the tree.
    std::unique_ptr<TreeRenderer> m_renderer;
#if defined(OAKULAR_HAS_SERVER)
    //! \brief Server for visualizing the client's running tree.
    std::shared_ptr<Server> m_server;
#endif
    //! \brief Modification flag for unsaved changes.
    bool m_is_modified = false;
    //! \brief Whether a document is open, hence whether edition is allowed.
    bool m_has_document = false;
    //! \brief Flag to open the "discard unsaved changes" confirmation.
    bool m_show_new_confirmation = false;
    //! \brief Currently opened behavior tree file path.
    std::string m_behavior_tree_filepath;
    //! \brief Available tree views (name -> TreeView)
    std::map<std::string, TreeView> m_tree_views;
    //! \brief Nodes for all trees and subtrees.
    std::unordered_map<ID, Node> m_nodes;
    //! \brief Auto-incremented unique node ID.
    ID m_unique_node_id = 1;
    //! \brief Selected node ID. When several nodes are selected this is the
    //! last one clicked, the one the edition popup works on.
    ID m_selected_node_id = -1;
    //! \brief Every selected node, holding \c m_selected_node_id when not
    //! empty.
    std::unordered_set<ID> m_selected_nodes;
    //! \brief Whether the layout follows the structure at each change.
    bool m_auto_layout = true;
    //! \brief Active tree view name
    std::string m_active_tree_name;
    //! \brief Flag to request programmatic tab change (used once then reset)
    bool m_request_tab_change = false;
    //! \brief Pending link creation from drag-drop
    ID m_pending_link_from_node = -1;
    //! \brief Palettes and popups state
    struct ShowPalettes
    {
        //! \brief Show the node creation palette.
        bool node_creation = false;
        //! \brief Show the node edition popup.
        bool node_edition = false;
        //! \brief Position of the palette (screen coordinates).
        ImVec2 position;
        //! \brief Position for node creation (canvas coordinates).
        ImVec2 canvas_position;
    } m_show_palettes;
    //! \brief Blackboard for storing shared variables.
    bt::Blackboard::Ptr m_blackboard;
    //! \brief Flag to show the blackboard panel.
    bool m_show_blackboard_panel = true;
    //! \brief DFS order of node IDs for visualizer mode (maps index -> node_id)
    std::vector<ID> m_dfs_node_order;
    //! \brief Node types offered by the creation palette.
    std::vector<NodeType> m_node_types;
    //! \brief Editing state of the node edition popup.
    struct NodeEditState
    {
        //! \brief Copy of the edited node, applied on validation.
        Node node;
        //! \brief Whether \c node holds a valid copy.
        bool initialized = false;
        //! \brief Input port name being typed.
        std::string new_input;
        //! \brief Output port name being typed.
        std::string new_output;
        //! \brief SubTree definition name being typed.
        std::string subtree_name;
        //! \brief Attribute name being typed.
        std::string new_attribute;
    } m_node_edit;

    // ------------------------------------------------------------------------
    //! \brief Editing state of the blackboard panel. Held per instance so that
    //! several editors may coexist in the same host.
    // ------------------------------------------------------------------------
    struct BlackboardPanelState
    {
        //! \brief Name of the variable being created.
        std::string new_var_name;
        //! \brief Value of the variable being created.
        std::string new_var_value;
        //! \brief Type index of the variable being created.
        int new_var_type = 0;
        //! \brief Text being typed for each edited entry, keyed by its path.
        std::unordered_map<std::string, std::string> edit_buffers;
        //! \brief Whether the "add field to struct" popup shall open.
        bool add_field_open = false;
        //! \brief Dotted path of the struct receiving the new field.
        std::string add_field_parent_path;
        //! \brief Name of the field being created.
        std::string add_field_name;
        //! \brief Value of the field being created.
        std::string add_field_value;
        //! \brief Type index of the field being created.
        int add_field_type = 0;
        //! \brief Whether the keys inherited from the parent scope are listed
        //! below the local ones. Only a subtree scope has a parent.
        bool show_inherited = true;
        //! \brief Whether the "new variable" form is unfolded.
        bool add_variable_open = false;
    } m_blackboard_panel;
};

} // namespace oakular
