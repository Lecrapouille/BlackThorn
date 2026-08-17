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
        //! \brief Blackboard input parameters
        std::vector<std::string> inputs;
        //! \brief Blackboard output parameters
        std::vector<std::string> outputs;
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
    //! \brief Handle the editor keyboard shortcuts (Ctrl+O, Ctrl+S, Ctrl+L,
    //! Ctrl+Q). Already called by \c draw.
    // ------------------------------------------------------------------------
    void handleKeyboardShortcuts();

    // ========================================================================
    // Tree edition.
    // ========================================================================

    // ------------------------------------------------------------------------
    //! \brief Reset the editor to an empty tree with a fresh blackboard.
    // ------------------------------------------------------------------------
    void reset();

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
    //! \brief Load a tree from a YAML file.
    //! \param p_filepath The path to the YAML file.
    // ------------------------------------------------------------------------
    void loadFromYaml(std::string const& p_filepath);

    // ------------------------------------------------------------------------
    //! \brief Load a tree from a YAML string (for visualizer mode).
    //! \param p_yaml_content The YAML content as a string.
    // ------------------------------------------------------------------------
    void loadFromYamlString(std::string const& p_yaml_content);

    // ------------------------------------------------------------------------
    //! \brief Save a tree to a YAML file.
    //! \param p_filepath The path to the YAML file.
    // ------------------------------------------------------------------------
    void saveToYaml(std::string const& p_filepath);

    // ------------------------------------------------------------------------
    //! \brief Auto-layout the nodes in the tree.
    // ------------------------------------------------------------------------
    void autoLayoutNodes();

    // ------------------------------------------------------------------------
    //! \brief Toggle the expansion state of a SubTree node.
    //! \param p_node_id The ID of the SubTree node.
    // ------------------------------------------------------------------------
    void toggleSubTreeExpansion(ID const p_node_id);

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
    //! \brief Get the edited blackboard, shareable with a running tree.
    // ------------------------------------------------------------------------
    [[nodiscard]] bt::Blackboard::Ptr blackboard() const
    {
        return m_blackboard;
    }

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

    //! \brief Draw the context menu for node operations.
    void showNodeContextMenu();

    //! \brief Draw the modal popup for editing node properties.
    void showNodeEditPopup();

    //! \brief Draw the visualizer status line (TCP server state).
    void showVisualizerPanel();

    //! \brief Handle edit mode interactions (node selection, link creation).
    void handleEditModeInteractions();

protected: // TreeView helpers

    //! \brief Get the current tree view (creates one if needed).
    TreeView& getCurrentTreeView();

    //! \brief Find a tree view by its root ID.
    TreeView* findTreeViewByRootId(ID p_root_id);

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

    //! \brief Check whether a node type may declare blackboard ports.
    [[nodiscard]] bool canHaveBlackboardPorts(std::string const& p_type) const;

private: // SubTree management (internal)

    bool expandSubTree(Node* p_subtree_node);
    bool collapseSubTree(Node* p_subtree_node);

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

    void layoutNodeRecursive(Node* p_node,
                             float p_x,
                             float p_y,
                             float& p_max_extent);

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
    //! \brief Currently opened behavior tree file path.
    std::string m_behavior_tree_filepath;
    //! \brief Available tree views (name -> TreeView)
    std::map<std::string, TreeView> m_tree_views;
    //! \brief Nodes for all trees and subtrees.
    std::unordered_map<ID, Node> m_nodes;
    //! \brief Auto-incremented unique node ID.
    ID m_unique_node_id = 1;
    //! \brief Selected node ID.
    ID m_selected_node_id = -1;
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
    } m_blackboard_panel;
};

} // namespace oakular
