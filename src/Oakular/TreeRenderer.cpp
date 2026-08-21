/**
 * @file TreeRenderer.cpp
 * @brief Custom node rendering implementation
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/BlackThorn
 */

#include "TreeRenderer.hpp"
#include "Editor.hpp"

#include "BlackThorn/Blackboard/Blackboard.hpp"

#include <imgui.h>

#include <cmath>
#include <iomanip>
#include <sstream>

namespace oakular {

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
static constexpr float PIN_RADIUS = 5.0f;
static constexpr float NODE_WIDTH = 180.0f;
static constexpr float NODE_PADDING = 8.0f;
static constexpr float NODE_ROUNDING = 4.0f;
static constexpr float LINK_THICKNESS = 2.0f;
static constexpr float GRID_SIZE = 32.0f;
static constexpr size_t MAX_VALUE_DISPLAY_LENGTH = 15;

// ----------------------------------------------------------------------------
// Helper function to get a display string from the blackboard
// ----------------------------------------------------------------------------
static std::string getBlackboardValueString(bt::Blackboard const* blackboard,
                                            const std::string& key)
{
    std::string result = bt::Blackboard::displayKey(blackboard, key);
    if (result.length() > MAX_VALUE_DISPLAY_LENGTH)
    {
        result = result.substr(0, MAX_VALUE_DISPLAY_LENGTH - 3) + "...";
    }
    return result;
}

// ----------------------------------------------------------------------------
//! \brief One line of the port list of a node: the port name, then what it is
//! bound to. A ${key} binding shows the blackboard value behind the key, a
//! literal shows itself.
// ----------------------------------------------------------------------------
static std::string formatPort(bt::Blackboard const* p_blackboard,
                              Editor::Port const& p_port)
{
    std::string line = "  - " + p_port.name;

    bool const is_reference = (p_port.binding.size() > 3U) &&
                              (p_port.binding.compare(0, 2, "${") == 0) &&
                              (p_port.binding.back() == '}');
    if (!is_reference)
    {
        if (!p_port.binding.empty())
        {
            line += " = " + p_port.binding;
        }
        return line;
    }

    std::string const key =
        p_port.binding.substr(2U, p_port.binding.size() - 3U);
    if (key != p_port.name)
    {
        line += " -> " + key;
    }

    std::string const value = getBlackboardValueString(p_blackboard, key);
    if (!value.empty())
    {
        line += ": " + value;
    }
    return line;
}

// ----------------------------------------------------------------------------
// Color lookup table for node types
// ----------------------------------------------------------------------------
static const std::unordered_map<std::string, ImU32> s_type_colors = {
    {"Sequence", IM_COL32(100, 150, 200, 255)},
    {"Selector", IM_COL32(200, 150, 100, 255)},
    {"Parallel", IM_COL32(150, 200, 150, 255)},
    {"Decorator", IM_COL32(200, 100, 200, 255)},
    {"Inverter", IM_COL32(200, 100, 200, 255)},
    {"Repeat", IM_COL32(200, 100, 200, 255)},
    {"UntilSuccess", IM_COL32(200, 100, 200, 255)},
    {"UntilFailure", IM_COL32(200, 100, 200, 255)},
    {"ForceSuccess", IM_COL32(200, 100, 200, 255)},
    {"ForceFailure", IM_COL32(200, 100, 200, 255)},
    {"RunOnce", IM_COL32(200, 100, 200, 255)},
    {"Timeout", IM_COL32(200, 100, 200, 255)},
    {"Delay", IM_COL32(200, 100, 200, 255)},
    {"Cooldown", IM_COL32(200, 100, 200, 255)},
    {"Action", IM_COL32(100, 200, 100, 255)},
    {"Condition", IM_COL32(200, 200, 100, 255)},
    {"Wait", IM_COL32(200, 200, 100, 255)},
    {"SetBlackboard", IM_COL32(200, 200, 100, 255)},
    {"SubTree", IM_COL32(150, 100, 200, 255)},
};

// ----------------------------------------------------------------------------
bool TreeRenderer::acceptsChildren(Editor::Node const& p_node) const
{
    if (!p_node.children.empty())
        return true;

    if (m_accepts_children)
        return m_accepts_children(p_node.type);

    return p_node.type == "Sequence" || p_node.type == "Selector" ||
           p_node.type == "Parallel" || p_node.type == "Inverter" ||
           p_node.type == "SubTree";
}

// ----------------------------------------------------------------------------
void TreeRenderer::shutdown()
{
    m_node_visuals.clear();
}

// ----------------------------------------------------------------------------
void TreeRenderer::drawBehaviorTree(
    std::unordered_map<ID, Editor::Node>& p_nodes,
    std::vector<Editor::Link> const& p_links,
    int p_layout_direction,
    bt::Blackboard* p_blackboard,
    bool p_read_only)
{
    m_layout_direction = p_layout_direction;
    m_blackboard = p_blackboard;
    // Reset frame state
    m_link_created_this_frame = false;
    m_link_deleted_this_frame = false;
    m_link_dropped_in_void = false;
    m_toggled_subtree_id = -1;
    m_box_select.released = false;
    // Clear visuals to only keep nodes from current tree view
    m_node_visuals.clear();

    // Begin canvas
    ImGui::BeginChild("Canvas",
                      ImVec2(0, 0),
                      true,
                      ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    m_canvas_size = ImGui::GetContentRegionAvail();
    m_canvas_pos = ImGui::GetCursorScreenPos();

    // ImGui reports the mouse buttons globally, so every interaction below has
    // to check that the pointer really is over this canvas. Without it, a click
    // in the blackboard panel or in a popup reaches the graph too.
    m_canvas_hovered =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                               ImGuiHoveredFlags_AllowWhenBlockedByPopup);

    handleCanvasPanAndZoom();
    drawGrid(draw_list, m_canvas_pos);

    // Clip rendering to canvas area
    draw_list->PushClipRect(m_canvas_pos,
                            ImVec2(m_canvas_pos.x + m_canvas_size.x,
                                   m_canvas_pos.y + m_canvas_size.y),
                            true);

    // First pass: calculate node visuals and positions
    for (auto const& [id, node] : p_nodes)
    {
        NodeVisual& visual = m_node_visuals[id];
        visual.size = calculateNodeSize(node);
        visual.position = canvasToScreen(node.position);
        visual.bounds = ImRect(visual.position,
                               ImVec2(visual.position.x + visual.size.x,
                                      visual.position.y + visual.size.y));

        // All nodes have input pins (including root for visual consistency)
        bool has_input = true;
        bool has_output = acceptsChildren(node);
        bool is_top_to_bottom =
            (p_layout_direction == 1); // 1 = TopToBottom, 0 = LeftToRight
        calculatePinPositions(visual, has_input, has_output, is_top_to_bottom);
    }

    // Second pass: draw links (below nodes)
    for (auto const& link : p_links)
    {
        auto from_it = m_node_visuals.find(link.from_node);
        auto to_it = m_node_visuals.find(link.to_node);

        if (from_it != m_node_visuals.end() && to_it != m_node_visuals.end())
        {
            ImVec2 start = from_it->second.output_pin_pos;
            ImVec2 end = to_it->second.input_pin_pos;
            bool is_selected = (link.from_node == m_selected_link_from &&
                                link.to_node == m_selected_link_to);
            bool is_top_to_bottom =
                (p_layout_direction == 1); // 1 = TopToBottom, 0 = LeftToRight
            drawLink(start, end, is_selected, is_top_to_bottom);
        }
    }

    // Draw temporary link during drag
    if (m_drag_state.is_dragging_link && m_drag_state.link_start_node >= 0)
    {
        auto it = m_node_visuals.find(m_drag_state.link_start_node);
        if (it != m_node_visuals.end())
        {
            ImVec2 start = it->second.output_pin_pos;
            ImVec2 end = ImGui::GetMousePos();
            bool is_top_to_bottom =
                (p_layout_direction == 1); // 1 = TopToBottom, 0 = LeftToRight
            drawLink(start, end, false, is_top_to_bottom);
        }
    }

    // Third pass: draw nodes
    bool is_top_to_bottom =
        (p_layout_direction ==
         static_cast<int>(Editor::LayoutDirection::TopToBottom));
    for (auto& [id, node] : p_nodes)
    {
        drawNode(node, is_top_to_bottom);
    }

    // Handle interactions in edit mode
    if (!p_read_only)
    {
        for (auto& [id, node] : p_nodes)
        {
            handleNodeDrag(node, !p_read_only);
            handleLinkCreation(node, !p_read_only);
        }

        handleBoxSelection(p_nodes);
    }

    draw_list->PopClipRect();
    ImGui::EndChild();
}

// ----------------------------------------------------------------------------
bool TreeRenderer::isOverAnyNode(ImVec2 p_mouse_pos) const
{
    for (auto const& [id, visual] : m_node_visuals)
    {
        if (visual.bounds.Contains(p_mouse_pos) ||
            isPinHovered(visual.input_pin_pos, p_mouse_pos) ||
            isPinHovered(visual.output_pin_pos, p_mouse_pos))
        {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
void TreeRenderer::handleBoxSelection(
    std::unordered_map<ID, Editor::Node> const& p_nodes)
{
    ImVec2 const mouse_pos = ImGui::GetMousePos();

    // A drag started on empty canvas is a rubber band. Starting it on a node
    // would fight with moving that node.
    if (!m_box_select.active && m_canvas_hovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !m_drag_state.is_dragging_node && !m_drag_state.is_dragging_link &&
        !isOverAnyNode(mouse_pos))
    {
        m_box_select.active = true;
        m_box_select.origin = mouse_pos;
    }

    if (!m_box_select.active)
        return;

    ImRect const box(std::min(m_box_select.origin.x, mouse_pos.x),
                     std::min(m_box_select.origin.y, mouse_pos.y),
                     std::max(m_box_select.origin.x, mouse_pos.x),
                     std::max(m_box_select.origin.y, mouse_pos.y));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(box.Min, box.Max, IM_COL32(255, 255, 100, 30));
    draw_list->AddRect(box.Min, box.Max, IM_COL32(255, 255, 100, 160));

    // Released outside the canvas too: dropping the band elsewhere must not
    // leave it stuck on screen.
    if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        return;

    m_box_select.active = false;
    m_box_select.released = true;
    m_box_select.nodes.clear();

    for (auto const& [id, node] : p_nodes)
    {
        auto const it = m_node_visuals.find(id);
        if ((it != m_node_visuals.end()) && box.Overlaps(it->second.bounds))
        {
            m_box_select.nodes.push_back(id);
        }
    }
}

// ----------------------------------------------------------------------------
bool TreeRenderer::getBoxSelection(std::vector<ID>& p_nodes) const
{
    if (!m_box_select.released)
        return false;

    p_nodes = m_box_select.nodes;
    return true;
}

// ----------------------------------------------------------------------------
void TreeRenderer::drawNode(Editor::Node const& p_node,
                            bool /*p_is_top_to_bottom*/)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    NodeVisual const& visual = m_node_visuals[p_node.id];

    ImVec2 pos = visual.position;
    ImVec2 size = visual.size;

    // Determine colors based on runtime status (visualizer mode)
    ImU32 bg_color;
    ImU32 header_color;

    if (p_node.runtime_status != 0) // Visualizer mode - color whole node
    {
        switch (p_node.runtime_status)
        {
            case 1: // RUNNING - Orange/Yellow
                bg_color = IM_COL32(80, 60, 20, 255);
                header_color = IM_COL32(255, 180, 0, 255);
                break;
            case 2: // SUCCESS - Green
                bg_color = IM_COL32(20, 60, 20, 255);
                header_color = IM_COL32(50, 200, 50, 255);
                break;
            case 3: // FAILURE - Red
                bg_color = IM_COL32(60, 20, 20, 255);
                header_color = IM_COL32(200, 50, 50, 255);
                break;
            default: // INVALID - Gray
                bg_color = IM_COL32(40, 40, 40, 255);
                header_color = IM_COL32(80, 80, 80, 255);
                break;
        }
    }
    else // Editor mode - use type color
    {
        bg_color = IM_COL32(45, 45, 48, 255);
        header_color = getNodeColor(p_node.type);
    }

    // Background
    draw_list->AddRectFilled(
        pos, ImVec2(pos.x + size.x, pos.y + size.y), bg_color, NODE_ROUNDING);

    // Header
    float header_height = 24.0f;
    draw_list->AddRectFilled(pos,
                             ImVec2(pos.x + size.x, pos.y + header_height),
                             header_color,
                             NODE_ROUNDING,
                             ImDrawFlags_RoundCornersTop);

    // Type text in header
    auto text_pos = ImVec2(pos.x + NODE_PADDING, pos.y + 4);
    draw_list->AddText(
        text_pos, IM_COL32(255, 255, 255, 255), p_node.type.c_str());

    // SubTree expand/collapse indicator in top right
    if (p_node.type == "SubTree" && !p_node.subtree_reference.empty())
    {
        const char* expand_text = p_node.is_expanded ? "[-]" : "[+]";
        auto button_pos = ImVec2(pos.x + size.x - 30, pos.y + 4);
        draw_list->AddText(
            button_pos, IM_COL32(150, 200, 255, 255), expand_text);
    }

    // Node name
    text_pos.y += header_height + NODE_PADDING;
    draw_list->AddText(
        text_pos, IM_COL32(220, 220, 220, 255), p_node.name.c_str());
    text_pos.y += 18;

    // SubTree reference
    if (p_node.type == "SubTree" && !p_node.subtree_reference.empty())
    {
        std::string ref_text = "Ref: " + p_node.subtree_reference;
        draw_list->AddText(
            text_pos, IM_COL32(180, 180, 180, 200), ref_text.c_str());
        text_pos.y += 18;
    }

    // Inputs. On a SubTree node a port is a remapping: the key of the nested
    // scope on the left, the parent key it reads from on the right.
    bool const is_subtree = (p_node.type == "SubTree");

    if (!p_node.inputs.empty())
    {
        text_pos.y += 4;
        draw_list->AddText(text_pos,
                           IM_COL32(150, 200, 255, 255),
                           is_subtree ? "In remapping:" : "Inputs:");
        text_pos.y += 16;
        for (const auto& input : p_node.inputs)
        {
            std::string const input_text = formatPort(m_blackboard, input);
            draw_list->AddText(
                text_pos, IM_COL32(180, 180, 180, 255), input_text.c_str());
            text_pos.y += 16;
        }
    }

    // Outputs
    if (!p_node.outputs.empty())
    {
        text_pos.y += 4;
        draw_list->AddText(text_pos,
                           IM_COL32(255, 200, 150, 255),
                           is_subtree ? "Out remapping:" : "Outputs:");
        text_pos.y += 16;
        for (const auto& output : p_node.outputs)
        {
            std::string const output_text = formatPort(m_blackboard, output);
            draw_list->AddText(
                text_pos, IM_COL32(180, 180, 180, 255), output_text.c_str());
            text_pos.y += 16;
        }
    }

    // Border
    ImU32 border_color;
    float border_thickness;

    if (m_selection.count(p_node.id) > 0U)
    {
        // Selected node gets a bright border
        border_color = IM_COL32(255, 255, 100, 255);
        border_thickness = 3.0f;
    }
    else if (p_node.runtime_status != 0) // Visualizer mode
    {
        // Match header color for a cohesive look
        border_color = header_color;
        border_thickness = 2.0f;
    }
    else
    {
        border_color = IM_COL32(100, 100, 100, 255);
        border_thickness = 1.5f;
    }

    draw_list->AddRect(pos,
                       ImVec2(pos.x + size.x, pos.y + size.y),
                       border_color,
                       NODE_ROUNDING,
                       0,
                       border_thickness);

    // Draw pins
    ImVec2 mouse_pos = ImGui::GetMousePos();

    // Input pin (all nodes have input pins for visual consistency)
    bool hovered = isPinHovered(visual.input_pin_pos, mouse_pos);
    drawPin(visual.input_pin_pos, true, hovered);

    // Output pin (nodes that can have children)
    if (acceptsChildren(p_node))
    {
        hovered = isPinHovered(visual.output_pin_pos, mouse_pos);
        drawPin(visual.output_pin_pos, false, hovered);
    }
}

// ----------------------------------------------------------------------------
void TreeRenderer::drawPin(ImVec2 p_position,
                           bool p_is_input,
                           bool p_is_hovered) const
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImU32 color = p_is_input ? IM_COL32(100, 200, 100, 255)  // Green for input
                             : IM_COL32(200, 100, 100, 255); // Red for output

    if (p_is_hovered)
    {
        color = IM_COL32(255, 255, 100, 255); // Yellow when hovered
    }

    // Filled circle
    draw_list->AddCircleFilled(p_position, PIN_RADIUS, color, 12);
    // Border
    draw_list->AddCircle(
        p_position, PIN_RADIUS, IM_COL32(255, 255, 255, 255), 12, 1.5f);
}

// ----------------------------------------------------------------------------
void TreeRenderer::drawLink(ImVec2 p_start,
                            ImVec2 p_end,
                            bool p_is_selected,
                            bool p_is_top_to_bottom) const
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Calculate control points for Bezier curve based on layout direction
    ImVec2 cp1;
    ImVec2 cp2;
    if (p_is_top_to_bottom)
    {
        // Vertical control
        float offset = std::abs(p_end.y - p_start.y) * 0.5f;
        cp1 = ImVec2(p_start.x, p_start.y + offset);
        cp2 = ImVec2(p_end.x, p_end.y - offset);
    }
    else
    {
        // Horizontal control
        float offset = std::abs(p_end.x - p_start.x) * 0.5f;
        cp1 = ImVec2(p_start.x + offset, p_start.y);
        cp2 = ImVec2(p_end.x - offset, p_end.y);
    }

    ImU32 color = p_is_selected ? IM_COL32(255, 200, 0, 255)
                                : IM_COL32(150, 150, 150, 255);

    float thickness = p_is_selected ? 3.0f : LINK_THICKNESS;

    draw_list->AddBezierCubic(p_start, cp1, cp2, p_end, color, thickness);
}

// ----------------------------------------------------------------------------
void TreeRenderer::drawGrid(ImDrawList* draw_list, ImVec2 canvas_pos) const
{
    ImU32 grid_color = IM_COL32(60, 60, 60, 100);

    // Calculate grid offset based on canvas offset
    float grid_offset_x = std::fmod(m_canvas_offset.x, GRID_SIZE);
    float grid_offset_y = std::fmod(m_canvas_offset.y, GRID_SIZE);

    // Vertical lines
    for (float x = grid_offset_x; x < m_canvas_size.x; x += GRID_SIZE)
    {
        draw_list->AddLine(
            ImVec2(canvas_pos.x + x, canvas_pos.y),
            ImVec2(canvas_pos.x + x, canvas_pos.y + m_canvas_size.y),
            grid_color);
    }

    // Horizontal lines
    for (float y = grid_offset_y; y < m_canvas_size.y; y += GRID_SIZE)
    {
        draw_list->AddLine(
            ImVec2(canvas_pos.x, canvas_pos.y + y),
            ImVec2(canvas_pos.x + m_canvas_size.x, canvas_pos.y + y),
            grid_color);
    }
}

// ----------------------------------------------------------------------------
void TreeRenderer::handleCanvasPanAndZoom()
{
    auto const& io = ImGui::GetIO();

    // Pan with middle mouse button. Started over the canvas only, so that
    // dragging in another window does not scroll the graph, but kept alive
    // afterwards so that the pointer may leave while panning.
    if (m_panning && !ImGui::IsMouseDown(ImGuiMouseButton_Middle))
    {
        m_panning = false;
    }
    else if (!m_panning && m_canvas_hovered &&
             ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
    {
        m_panning = true;
    }

    if (m_panning && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        ImVec2 delta = io.MouseDelta;
        m_canvas_offset.x += delta.x;
        m_canvas_offset.y += delta.y;
    }

    // Zoom with mouse wheel (optional for v1)
    // if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f)
    // {
    //     m_canvas_zoom += io.MouseWheel * 0.1f;
    //     m_canvas_zoom = std::max(0.5f, std::min(2.0f, m_canvas_zoom));
    // }
}

// ----------------------------------------------------------------------------
void TreeRenderer::handleNodeDrag(Editor::Node& node, bool is_edit_mode)
{
    if (!is_edit_mode)
        return;

    NodeVisual const& visual = m_node_visuals[node.id];
    ImVec2 mouse_pos = ImGui::GetMousePos();

    // Check if mouse is over the entire node (but not on pins)
    if (m_canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        visual.bounds.Contains(mouse_pos))
    {
        // Don't start dragging if clicking on pins
        if (!isPinHovered(visual.input_pin_pos, mouse_pos) &&
            !isPinHovered(visual.output_pin_pos, mouse_pos))
        {
            // Start dragging
            m_drag_state.is_dragging_node = true;
            m_drag_state.dragged_node_id = node.id;
            m_drag_state.mouse_offset = ImVec2(mouse_pos.x - visual.position.x,
                                               mouse_pos.y - visual.position.y);
        }
    }

    // Continue dragging
    if (m_drag_state.is_dragging_node &&
        m_drag_state.dragged_node_id == node.id)
    {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            auto new_screen_pos =
                ImVec2(mouse_pos.x - m_drag_state.mouse_offset.x,
                       mouse_pos.y - m_drag_state.mouse_offset.y);
            node.position = screenToCanvas(new_screen_pos);
        }
        else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            // Stop dragging
            m_drag_state.is_dragging_node = false;
            m_drag_state.dragged_node_id = -1;
        }
    }
}

// ----------------------------------------------------------------------------
void TreeRenderer::handleLinkCreation(Editor::Node const& p_node,
                                      bool p_is_edit_mode)
{
    if ((!p_is_edit_mode) || (!acceptsChildren(p_node)))
        return;

    NodeVisual const& visual = m_node_visuals[p_node.id];
    ImVec2 mouse_pos = ImGui::GetMousePos();

    // Check if mouse clicked on output pin
    if (m_canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (isPinHovered(visual.output_pin_pos, mouse_pos))
        {
            // Start link creation
            m_drag_state.is_dragging_link = true;
            m_drag_state.link_start_node = p_node.id;
        }
    }

    // Handle link release
    if (m_drag_state.is_dragging_link &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        bool link_connected = false;

        // Check if released on an input pin
        for (auto const& [id, other_node_visual] : m_node_visuals)
        {
            if (id != m_drag_state.link_start_node) // Can't link to self
            {
                if (isPinHovered(other_node_visual.input_pin_pos, mouse_pos))
                {
                    // Link created!
                    m_created_link_from = m_drag_state.link_start_node;
                    m_created_link_to = id;
                    m_link_created_this_frame = true;
                    link_connected = true;
                    break;
                }
            }
        }

        // If link was not connected to any pin, it was dropped in void
        if (!link_connected)
        {
            m_link_dropped_in_void = true;
            m_link_void_from_node = m_drag_state.link_start_node;
        }

        // Reset drag state
        m_drag_state.is_dragging_link = false;
        m_drag_state.link_start_node = -1;
    }

    // Handle SubTree expand/collapse button click
    if (p_node.type == "SubTree" && !p_node.subtree_reference.empty())
    {
        // Check click on expand/collapse button area (top right)
        auto button_pos = ImVec2(visual.position.x + visual.size.x - 30,
                                 visual.position.y + 4);
        auto button_rect =
            ImRect(button_pos, ImVec2(button_pos.x + 30, button_pos.y + 16));

        if (m_canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            button_rect.Contains(mouse_pos))
        {
            m_toggled_subtree_id = p_node.id;
        }
    }
}

// ----------------------------------------------------------------------------
bool TreeRenderer::isPinHovered(ImVec2 p_pin_pos, ImVec2 p_mouse_pos) const
{
    float dx = p_mouse_pos.x - p_pin_pos.x;
    float dy = p_mouse_pos.y - p_pin_pos.y;
    float dist_sq = dx * dx + dy * dy;
    return dist_sq <= (PIN_RADIUS + 2) * (PIN_RADIUS + 2);
}

// ----------------------------------------------------------------------------
ImVec2 TreeRenderer::calculateNodeSize(const Editor::Node& p_node) const
{
    float height = 24.0f + NODE_PADDING; // Header
    height += 18.0f;                     // Name

    // SubTree reference
    if (p_node.type == "SubTree" && !p_node.subtree_reference.empty())
    {
        height += 18.0f + 18.0f; // Reference + expand button
    }

    // Inputs
    if (!p_node.inputs.empty())
    {
        height += 20.0f; // "Inputs:" label
        height += float(p_node.inputs.size()) * 16.0f;
    }

    // Outputs
    if (!p_node.outputs.empty())
    {
        height += 20.0f; // "Outputs:" label
        height += float(p_node.outputs.size()) * 16.0f;
    }

    height += NODE_PADDING * 2; // Bottom padding

    return ImVec2(NODE_WIDTH, height);
}

// ----------------------------------------------------------------------------
void TreeRenderer::calculatePinPositions(NodeVisual& p_visual,
                                         bool p_has_input,
                                         bool p_has_output,
                                         bool p_is_top_to_bottom) const
{
    if (p_is_top_to_bottom)
    {
        // Input at top, centered horizontally
        if (p_has_input)
        {
            p_visual.input_pin_pos =
                ImVec2(p_visual.position.x + p_visual.size.x * 0.5f,
                       p_visual.position.y);
        }

        // Output at bottom, centered horizontally
        if (p_has_output)
        {
            p_visual.output_pin_pos =
                ImVec2(p_visual.position.x + p_visual.size.x * 0.5f,
                       p_visual.position.y + p_visual.size.y);
        }
    }
    else
    {
        // Input at left, centered vertically
        if (p_has_input)
        {
            p_visual.input_pin_pos =
                ImVec2(p_visual.position.x,
                       p_visual.position.y + p_visual.size.y * 0.5f);
        }

        // Output at right, centered vertically
        if (p_has_output)
        {
            p_visual.output_pin_pos =
                ImVec2(p_visual.position.x + p_visual.size.x,
                       p_visual.position.y + p_visual.size.y * 0.5f);
        }
    }
}

// ----------------------------------------------------------------------------
ImVec2 TreeRenderer::screenToCanvas(ImVec2 p_screen_pos) const
{
    return ImVec2(
        (p_screen_pos.x - m_canvas_pos.x - m_canvas_offset.x) / m_canvas_zoom,
        (p_screen_pos.y - m_canvas_pos.y - m_canvas_offset.y) / m_canvas_zoom);
}

// ----------------------------------------------------------------------------
ImVec2 TreeRenderer::canvasToScreen(ImVec2 p_canvas_pos) const
{
    return ImVec2(
        p_canvas_pos.x * m_canvas_zoom + m_canvas_offset.x + m_canvas_pos.x,
        p_canvas_pos.y * m_canvas_zoom + m_canvas_offset.y + m_canvas_pos.y);
}

// ----------------------------------------------------------------------------
ImU32 TreeRenderer::getNodeColor(const std::string& p_type) const
{
    auto it = s_type_colors.find(p_type);
    if (it != s_type_colors.end())
    {
        return it->second;
    }
    return IM_COL32(150, 150, 150, 255); // Default gray for unknown types
}

// ----------------------------------------------------------------------------
bool TreeRenderer::getLinkCreated(ID& p_from_node, ID& p_to_node) const
{
    if (m_link_created_this_frame)
    {
        p_from_node = m_created_link_from;
        p_to_node = m_created_link_to;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
bool TreeRenderer::getLinkDeleted(ID& p_from_node, ID& p_to_node) const
{
    if (m_link_deleted_this_frame)
    {
        p_from_node = m_deleted_link_from;
        p_to_node = m_deleted_link_to;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
bool TreeRenderer::getSubTreeToggled(ID& p_node_id) const
{
    if (m_toggled_subtree_id >= 0)
    {
        p_node_id = m_toggled_subtree_id;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
bool TreeRenderer::getLinkDroppedInVoid(ID& p_from_node) const
{
    if (m_link_dropped_in_void)
    {
        p_from_node = m_link_void_from_node;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
TreeRenderer::ID TreeRenderer::getHoveredNodeId() const
{
    if (!m_canvas_hovered)
        return -1;

    ImVec2 mouse_pos = ImGui::GetMousePos();

    // Check if mouse is over any node
    for (const auto& [id, visual] : m_node_visuals)
    {
        if (!visual.bounds.Contains(mouse_pos))
            continue;

        // A pin belongs to the linking gesture, not to the node: reporting the
        // node here would select it instead of starting the link.
        if (isPinHovered(visual.input_pin_pos, mouse_pos) ||
            isPinHovered(visual.output_pin_pos, mouse_pos))
            continue;

        return id;
    }

    return -1;
}

} // namespace oakular
