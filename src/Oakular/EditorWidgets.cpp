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
constexpr std::array<char const*, 5> c_value_type_names = {"string",
                                                           "int",
                                                           "double",
                                                           "bool",
                                                           "struct"};

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
    //! \brief Dotted path of the entry whose delete button was pressed.
    std::string remove_request;
    //! \brief Set when any value was edited during this frame.
    bool modified = false;
    //! \brief Whether the entries may be edited at all. A subtree scope is a
    //! preview of what the remapping will feed it, not a place to type in.
    bool read_only = false;
};

//! \brief Width of the type column of the blackboard table.
constexpr float c_type_column_width = 92.0f;
//! \brief Width of the column holding the per-entry buttons.
constexpr float c_action_column_width = 48.0f;

void drawBlackboardRow(std::string const& p_key,
                       BlackboardValue& p_value,
                       std::string const& p_full_path,
                       BlackboardDrawContext& p_context);

// ----------------------------------------------------------------------------
//! \brief Open the table the entries are laid out in: one row per entry, the
//! nested ones indented under their parent by the tree nodes.
// ----------------------------------------------------------------------------
bool beginBlackboardTable(char const* p_id)
{
    if (!ImGui::BeginTable(p_id,
                           4,
                           ImGuiTableFlags_Resizable |
                               ImGuiTableFlags_BordersInnerV |
                               ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_NoBordersInBodyUntilResize))
    {
        return false;
    }

    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.45f);
    ImGui::TableSetupColumn(
        "Type", ImGuiTableColumnFlags_WidthFixed, c_type_column_width);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.55f);
    ImGui::TableSetupColumn(
        "##actions", ImGuiTableColumnFlags_WidthFixed, c_action_column_width);
    ImGui::TableHeadersRow();
    return true;
}

// ----------------------------------------------------------------------------
//! \brief Fill the type column with the name of the type held by an entry.
// ----------------------------------------------------------------------------
void drawTypeCell(BlackboardValue const& p_value)
{
    ImGui::TableNextColumn();
    ImGui::TextColored(
        ImVec4(0.6f, 0.6f, 0.8f, 1.0f), "%s", valueTypeName(p_value).c_str());
}

// ----------------------------------------------------------------------------
//! \brief Draw the delete button of a row, asking for its removal.
// ----------------------------------------------------------------------------
void drawRemoveButton(std::string const& p_full_path,
                      BlackboardDrawContext& p_context)
{
    if (p_context.read_only)
        return;

    if (ImGui::SmallButton("X"))
    {
        p_context.remove_request = p_full_path;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Remove %s", p_full_path.c_str());
    }
}

// ----------------------------------------------------------------------------
//! \brief Draw an editable scalar value.
// ----------------------------------------------------------------------------
void drawScalarEntry(std::string const& p_key,
                     BlackboardValue& p_value,
                     std::string const& p_full_path,
                     BlackboardDrawContext& p_context)
{
    ImGui::TableNextColumn();
    ImGui::TreeNodeEx(
        p_key.c_str(),
        ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Bullet);

    drawTypeCell(p_value);

    ImGui::TableNextColumn();
    if (p_context.read_only)
    {
        ImGui::TextUnformatted(scalarToEditableText(p_value).c_str());
    }
    else
    {
        auto& buffer = p_context.edit_buffers;
        if (buffer.find(p_full_path) == buffer.end())
        {
            buffer[p_full_path] = scalarToEditableText(p_value);
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
        std::string const input_id = "##" + p_full_path;
        if (ImGui::InputText(input_id.c_str(),
                             &buffer[p_full_path],
                             ImGuiInputTextFlags_EnterReturnsTrue) &&
            assignScalarFromText(p_value, buffer[p_full_path]))
        {
            p_context.modified = true;
        }
    }

    ImGui::TableNextColumn();
    drawRemoveButton(p_full_path, p_context);
}

// ----------------------------------------------------------------------------
//! \brief Draw a struct (map) entry as a collapsible row.
// ----------------------------------------------------------------------------
void drawStructEntry(std::string const& p_key,
                     BlackboardValue& p_value,
                     std::string const& p_full_path,
                     BlackboardDrawContext& p_context)
{
    auto& map = std::get<BlackboardMap>(p_value.asBase());

    ImGui::TableNextColumn();
    bool const node_open =
        ImGui::TreeNodeEx(p_key.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);

    drawTypeCell(p_value);

    ImGui::TableNextColumn();
    ImGui::TextDisabled(
        "%zu field%s", map.size(), (map.size() == 1U) ? "" : "s");

    // The buttons sit in their own column: the tree node used to span the whole
    // row and swallow the clicks aimed at them.
    ImGui::TableNextColumn();
    if (!p_context.read_only)
    {
        if (ImGui::SmallButton("+"))
        {
            p_context.add_field_open = true;
            p_context.add_field_parent_path = p_full_path;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Add a field to %s", p_full_path.c_str());
        }
        ImGui::SameLine();
    }
    drawRemoveButton(p_full_path, p_context);

    if (!node_open)
        return;

    // Sorted so that the fields do not dance around from frame to frame: the
    // storage is an unordered map.
    std::vector<std::string> field_keys;
    field_keys.reserve(map.size());
    for (auto const& [field_key, field_value] : map)
    {
        field_keys.push_back(field_key);
    }
    std::sort(field_keys.begin(), field_keys.end());

    for (std::string const& field_key : field_keys)
    {
        ImGui::PushID(field_key.c_str());
        drawBlackboardRow(field_key,
                          map[field_key],
                          p_full_path + "." + field_key,
                          p_context);
        ImGui::PopID();
    }

    // A field asked to be removed: this is the only place holding its map.
    if (!p_context.remove_request.empty())
    {
        std::string const prefix = p_full_path + ".";
        if (p_context.remove_request.compare(0, prefix.size(), prefix) == 0)
        {
            std::string const field =
                p_context.remove_request.substr(prefix.size());
            if (field.find('.') == std::string::npos && map.erase(field) > 0U)
            {
                p_context.modified = true;
                p_context.remove_request.clear();
            }
        }
    }

    ImGui::TreePop();
}

// ----------------------------------------------------------------------------
//! \brief Draw an array entry as a collapsible row.
// ----------------------------------------------------------------------------
void drawArrayEntry(std::string const& p_key,
                    BlackboardValue& p_value,
                    std::string const& p_full_path,
                    BlackboardDrawContext& p_context)
{
    if (auto* doubles = std::get_if<std::vector<double>>(&p_value.asBase()))
    {
        auto& vec = *doubles;

        ImGui::TableNextColumn();
        bool const node_open =
            ImGui::TreeNodeEx(p_key.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);

        drawTypeCell(p_value);

        ImGui::TableNextColumn();
        ImGui::TextDisabled(
            "%zu item%s", vec.size(), (vec.size() == 1U) ? "" : "s");

        ImGui::TableNextColumn();
        drawRemoveButton(p_full_path, p_context);

        if (!node_open)
            return;

        for (size_t i = 0; i < vec.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            std::string const item_path =
                p_full_path + "[" + std::to_string(i) + "]";

            ImGui::TableNextColumn();
            ImGui::TreeNodeEx(("[" + std::to_string(i) + "]").c_str(),
                              ImGuiTreeNodeFlags_Leaf |
                                  ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                  ImGuiTreeNodeFlags_Bullet);

            ImGui::TableNextColumn();
            ImGui::TextDisabled("double");

            ImGui::TableNextColumn();
            if (p_context.read_only)
            {
                ImGui::Text("%g", vec[i]);
            }
            else
            {
                auto& buffer = p_context.edit_buffers;
                if (buffer.find(item_path) == buffer.end())
                {
                    buffer[item_path] = std::to_string(vec[i]);
                }

                ImGui::SetNextItemWidth(-FLT_MIN);
                std::string const input_id = "##" + item_path;
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
            }

            ImGui::TableNextColumn();
            ImGui::PopID();
        }

        ImGui::TreePop();
        return;
    }

    auto* items = std::get_if<BlackboardList>(&p_value.asBase());
    if (items == nullptr)
        return;

    auto& vec = *items;

    ImGui::TableNextColumn();
    bool const node_open =
        ImGui::TreeNodeEx(p_key.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);

    drawTypeCell(p_value);

    ImGui::TableNextColumn();
    ImGui::TextDisabled(
        "%zu item%s", vec.size(), (vec.size() == 1U) ? "" : "s");

    ImGui::TableNextColumn();
    drawRemoveButton(p_full_path, p_context);

    if (!node_open)
        return;

    for (size_t i = 0; i < vec.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        drawBlackboardRow("[" + std::to_string(i) + "]",
                          vec[i],
                          p_full_path + "[" + std::to_string(i) + "]",
                          p_context);
        ImGui::PopID();
    }

    ImGui::TreePop();
}

// ----------------------------------------------------------------------------
//! \brief Draw one row of the blackboard table, dispatching on the runtime type
//! of the entry. Assumes a table row was started by the caller.
// ----------------------------------------------------------------------------
void drawBlackboardRow(std::string const& p_key,
                       BlackboardValue& p_value,
                       std::string const& p_full_path,
                       BlackboardDrawContext& p_context)
{
    ImGui::TableNextRow();

    auto const& base = p_value.asBase();

    if (std::holds_alternative<BlackboardMap>(base))
    {
        drawStructEntry(p_key, p_value, p_full_path, p_context);
        return;
    }

    if (std::holds_alternative<BlackboardList>(base) ||
        std::holds_alternative<std::vector<double>>(base))
    {
        drawArrayEntry(p_key, p_value, p_full_path, p_context);
        return;
    }

    if (std::holds_alternative<std::monostate>(base) ||
        std::holds_alternative<std::any>(base))
    {
        // Host-owned C++ type, or nothing at all: the editor knows nothing
        // about the layout, so the row is read-only.
        ImGui::TableNextColumn();
        ImGui::TreeNodeEx(
            p_key.c_str(),
            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Bullet);

        drawTypeCell(p_value);

        ImGui::TableNextColumn();
        ImGui::TextDisabled(std::holds_alternative<std::any>(base) ? "<custom>"
                                                                   : "<null>");

        ImGui::TableNextColumn();
        drawRemoveButton(p_full_path, p_context);
        return;
    }

    drawScalarEntry(p_key, p_value, p_full_path, p_context);
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
            if (m_has_document)
            {
                showDocumentStatusBar();
                showEditorTabs();
                showAddNodePalette();
            }
            else
            {
                showNoDocumentPanel();
            }
            // Opened from the menu bar, where the ImGui ID stack belongs to the
            // menu: the popup itself has to live here.
            showNewDocumentConfirmation();
            break;
        case Mode::Visualizer:
            showVisualizerPanel();
            drawBehaviorTree();
            break;
    }

    ImGui::End();
}

// ----------------------------------------------------------------------------
void Editor::showNoDocumentPanel()
{
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                       "No behavior tree opened.");
    ImGui::Spacing();
    ImGui::TextDisabled(
        "Use File > New Behavior Tree to start from scratch, or\n"
        "File > Load Behavior Tree to open an existing YAML file.");
    ImGui::Spacing();

    if (ImGui::Button("New Behavior Tree"))
    {
        newDocument();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Behavior Tree..."))
    {
        onFileDialogRequested.emit(FileDialog::Load);
    }
}

// ----------------------------------------------------------------------------
void Editor::showDocumentStatusBar()
{
    // The tab bar only shows up once the document has a subtree, and the host
    // window title is not ours to set: without this line nothing would tell the
    // user that the tree holds unsaved changes.
    if (m_is_modified)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s", documentTitle().c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(unsaved changes)");
    }
    else
    {
        ImGui::TextDisabled("%s", documentTitle().c_str());
    }

    std::size_t const selected = m_selected_nodes.size();
    if (selected > 1U)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("| %zu nodes selected", selected);
    }

    ImGui::Separator();
}

// ----------------------------------------------------------------------------
void Editor::showNewDocumentConfirmation()
{
    if (m_show_new_confirmation)
    {
        ImVec2 const center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(
            center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Discard changes?");
        m_show_new_confirmation = false;
    }

    if (!ImGui::BeginPopupModal(
            "Discard changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    ImGui::Text("The current behavior tree has unsaved changes.");
    ImGui::Separator();

    if (ImGui::Button("Save first", ImVec2(140, 0)))
    {
        // An unnamed tree only gets a host save dialog here, so the new
        // document waits for the next explicit request rather than racing it.
        if (save())
        {
            newDocument();
        }
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Discard and start anew", ImVec2(180, 0)))
    {
        newDocument();
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
void Editor::drawMenuBar()
{
    // Everything acting on a tree stays greyed out until a document exists:
    // there is nothing to add a node to, nor anything to save.
    bool const creation_mode = (m_mode == Mode::Creation);
    bool const editable = isEditable();

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem(
                "New Behavior Tree", "Ctrl+N", false, creation_mode))
        {
            if (m_is_modified)
            {
                m_show_new_confirmation = true;
            }
            else
            {
                newDocument();
            }
        }

        // The editor cannot browse the file system on its own: the host owns
        // the file dialog and calls back loadFromYaml/saveToYaml.
        if (ImGui::MenuItem("Load Behavior Tree...", "Ctrl+O"))
        {
            onFileDialogRequested.emit(FileDialog::Load);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save", "Ctrl+S", false, editable))
        {
            save();
        }

        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, editable))
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
        // A toggle, not an action: when ticked, a node created or linked lands
        // at the place the structure gives it without further asking.
        if (ImGui::MenuItem("Auto Layout", nullptr, m_auto_layout, editable))
        {
            setAutoLayoutEnabled(!m_auto_layout);
        }

        if (ImGui::MenuItem("Layout Now", "Ctrl+L", false, editable))
        {
            autoLayoutNodes();
        }

        ImGui::Separator();

        // Define the layout direction to display the tree (left to right)
        if (ImGui::MenuItem("Layout: Left to Right",
                            nullptr,
                            getCurrentTreeView().layout_direction ==
                                LayoutDirection::LeftToRight,
                            editable))
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
                            editable))
        {
            getCurrentTreeView().layout_direction =
                LayoutDirection::TopToBottom;
            autoLayoutNodes();
        }

        ImGui::Separator();

        // Add a new node
        if (ImGui::MenuItem("Add Node", nullptr, false, editable))
        {
            m_show_palettes.node_creation = true;
            m_show_palettes.position = ImGui::GetMousePos();
            m_pending_link_from_node = -1;
        }

        if (ImGui::MenuItem("New SubTree", nullptr, false, editable))
        {
            std::string const name = createSubTreeDefinition("SubTree");
            m_active_tree_name = name;
            m_request_tab_change = true;
        }

        ImGui::Separator();

        if (ImGui::MenuItem(
                "Select All", "Ctrl+A", false, editable && !m_nodes.empty()))
        {
            std::unordered_set<ID> visible;
            for (ID root_id : collectViewRoots())
            {
                collectVisibleNodes(root_id, visible);
            }
            m_selected_nodes = visible;
            m_selected_node_id =
                visible.empty() ? -1 : *m_selected_nodes.begin();
        }

        if (ImGui::MenuItem("Delete Selection",
                            "Del",
                            false,
                            editable && !m_selected_nodes.empty()))
        {
            deleteSelection();
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

    bool const shift_pressed = ImGui::IsKeyDown(ImGuiKey_LeftShift) ||
                               ImGui::IsKeyDown(ImGuiKey_RightShift);

    if (ImGui::IsKeyPressed(ImGuiKey_N) && m_mode == Mode::Creation)
    {
        if (m_is_modified)
        {
            m_show_new_confirmation = true;
        }
        else
        {
            newDocument();
        }
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_O))
    {
        onFileDialogRequested.emit(FileDialog::Load);
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_S) && isEditable())
    {
        if (shift_pressed)
        {
            onFileDialogRequested.emit(FileDialog::Save);
        }
        else
        {
            save();
        }
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_L) && isEditable())
    {
        autoLayoutNodes();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_A) && isEditable())
    {
        std::unordered_set<ID> visible;
        for (ID root_id : collectViewRoots())
        {
            collectVisibleNodes(root_id, visible);
        }
        m_selected_nodes = visible;
        m_selected_node_id = visible.empty() ? -1 : *m_selected_nodes.begin();
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
            clearSelection();
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
    if (!isEditable())
    {
        m_show_palettes.node_creation = false;
        return;
    }

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
    // Skip unless a document is open in edit mode: without one there is
    // nothing to select, to link or to add a node to.
    if (!isEditable())
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

    // A rubber band dragged over empty canvas replaces the selection, or adds
    // to it when a modifier is held.
    std::vector<ID> boxed;
    if (m_renderer->getBoxSelection(boxed))
    {
        bool const additive = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
        if (!additive)
        {
            clearSelection();
        }
        for (ID id : boxed)
        {
            m_selected_nodes.insert(id);
            m_selected_node_id = id;
        }
        if (m_selected_nodes.empty())
        {
            m_selected_node_id = -1;
        }
    }

    // Every mouse test below reads the global ImGui state, so nothing happens
    // unless the pointer really is over the graph: a right click in the
    // blackboard panel used to pop the node palette up.
    bool const on_canvas = m_renderer->isCanvasHovered();

    // Get hovered node once per frame
    ID hovered_id = on_canvas ? m_renderer->getHoveredNodeId() : -1;

    // Double-click on node: SubTree -> open tab, other -> edit
    if (on_canvas && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
        hovered_id >= 0)
    {
        selectNode(hovered_id);
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
    // Single click: select the node, or add it to the selection with Ctrl.
    else if (on_canvas && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift)
        {
            toggleNodeSelection(hovered_id);
        }
        else if (hovered_id >= 0)
        {
            // Keep a multiple selection alive when clicking inside it: this is
            // what lets the user drag or extract the whole group.
            if (m_selected_nodes.count(hovered_id) == 0U)
            {
                selectNode(hovered_id);
            }
            else
            {
                m_selected_node_id = hovered_id;
            }
        }
        // Clicking empty canvas starts a rubber band; the selection is cleared
        // when it is released, so nothing to do here.
    }

    // Right-click: context menu on node, or add palette on empty space
    if (on_canvas && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        if (hovered_id >= 0)
        {
            if (m_selected_nodes.count(hovered_id) == 0U)
            {
                selectNode(hovered_id);
            }
            else
            {
                m_selected_node_id = hovered_id;
            }
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
    if (!m_selected_nodes.empty() && ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        deleteSelection();
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

    bool const is_subtree_call = (selected->type == "SubTree");
    std::size_t const selection_size = m_selected_nodes.size();

    if (selection_size > 1U)
    {
        ImGui::TextDisabled("%zu nodes selected", selection_size);
        ImGui::Separator();
    }

    // Go to definition for SubTree nodes
    if (is_subtree_call && !selected->subtree_reference.empty())
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

    // No interaction selects a link, so this is the only way to break one
    // without deleting a node.
    if (ImGui::MenuItem(
            "Detach from parent", nullptr, false, selected->parent >= 0))
    {
        deleteLink(selected->parent, m_selected_node_id);
        ImGui::CloseCurrentPopup();
    }

    ImGui::Separator();

    // Extracting a branch, and putting it back: the two halves of factoring a
    // piece of behavior out into its own reusable tree.
    if (!is_subtree_call)
    {
        if (ImGui::MenuItem("Extract to SubTree"))
        {
            convertToSubTree(m_selected_node_id);
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("Move this node and its children into a new "
                              "SubTree definition, and call it from here.");
        }
    }
    else
    {
        bool const shared = countSubTreeReferences(selected->subtree_reference,
                                                   m_selected_node_id) > 0U;
        if (ImGui::MenuItem("Inline SubTree", nullptr, false, !shared))
        {
            inlineSubTree(m_selected_node_id);
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(shared ? "Another SubTree node calls this "
                                       "definition: inlining would break it."
                                     : "Replace this node by the nodes of its "
                                       "definition.");
        }
    }

    ImGui::Separator();

    char const* delete_label =
        (selection_size > 1U) ? "Delete selection" : "Delete";
    if (ImGui::MenuItem(delete_label, "Del"))
    {
        deleteSelection();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
void Editor::showPortTable(char const* p_label,
                           std::vector<Port>& p_ports,
                           std::string& p_new_name,
                           bool const p_is_remapping)
{
    ImGui::PushID(p_label);
    ImGui::Text("%s:", p_label);

    if (!p_ports.empty() &&
        ImGui::BeginTable("##ports",
                          3,
                          ImGuiTableFlags_SizingStretchProp |
                              ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn(p_is_remapping ? "Subtree key" : "Port",
                                ImGuiTableColumnFlags_WidthStretch,
                                0.4f);
        ImGui::TableSetupColumn(p_is_remapping ? "Parent expression"
                                               : "Bound to",
                                ImGuiTableColumnFlags_WidthStretch,
                                0.5f);
        ImGui::TableSetupColumn(
            "##remove", ImGuiTableColumnFlags_WidthFixed, 24.0f);
        ImGui::TableHeadersRow();

        std::size_t doomed = p_ports.size();
        for (std::size_t i = 0; i < p_ports.size(); ++i)
        {
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(i));

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##name", &p_ports[i].name);

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##binding", &p_ports[i].binding);

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("X"))
            {
                doomed = i;
            }

            ImGui::PopID();
        }

        ImGui::EndTable();

        if (doomed < p_ports.size())
        {
            p_ports.erase(p_ports.begin() +
                          static_cast<std::ptrdiff_t>(doomed));
        }
    }

    ImGui::SetNextItemWidth(-90.0f);
    ImGui::InputText("##new_port", &p_new_name);
    ImGui::SameLine();
    if (ImGui::Button("Add", ImVec2(80.0f, 0.0f)) && !p_new_name.empty())
    {
        // Defaults to reading the key of the same name, the common case; the
        // user then edits the expression when a remapping is needed.
        p_ports.push_back({p_new_name, "${" + p_new_name + "}"});
        p_new_name.clear();
    }

    ImGui::PopID();
}

// ----------------------------------------------------------------------------
void Editor::showAttributeTable(
    std::vector<std::pair<std::string, std::string>>& p_attributes,
    std::string& p_new_name)
{
    ImGui::PushID("attributes");
    ImGui::Text("Attributes:");
    ImGui::SetItemTooltip("Keys passed to the engine as written, for instance "
                          "key and value on a SetBlackboard, times on a "
                          "Repeat, or the _id read from the file.");

    if (!p_attributes.empty() &&
        ImGui::BeginTable("##attributes",
                          3,
                          ImGuiTableFlags_SizingStretchProp |
                              ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn(
            "Key", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableSetupColumn(
            "Value", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn(
            "##remove", ImGuiTableColumnFlags_WidthFixed, 24.0f);
        ImGui::TableHeadersRow();

        std::size_t doomed = p_attributes.size();
        for (std::size_t i = 0; i < p_attributes.size(); ++i)
        {
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(i));

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##key", &p_attributes[i].first);

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##value", &p_attributes[i].second);

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("X"))
            {
                doomed = i;
            }

            ImGui::PopID();
        }

        ImGui::EndTable();

        if (doomed < p_attributes.size())
        {
            p_attributes.erase(p_attributes.begin() +
                               static_cast<std::ptrdiff_t>(doomed));
        }
    }

    ImGui::SetNextItemWidth(-90.0f);
    ImGui::InputText("##new_attribute", &p_new_name);
    ImGui::SameLine();
    if (ImGui::Button("Add", ImVec2(80.0f, 0.0f)) && !p_new_name.empty())
    {
        p_attributes.emplace_back(p_new_name, std::string());
        p_new_name.clear();
    }

    ImGui::PopID();
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
        m_node_edit.subtree_name.clear();
        m_node_edit.new_attribute.clear();

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
        bool const is_subtree = (edited.type == "SubTree");
        if (is_subtree)
        {
            ImGui::Spacing();
            ImGui::Text("Definition:");

            if (ImGui::BeginCombo("##AvailableSubTrees",
                                  edited.subtree_reference.empty()
                                      ? "(none)"
                                      : edited.subtree_reference.c_str()))
            {
                for (auto const& [name, view] : m_tree_views)
                {
                    if (!view.is_subtree)
                        continue;

                    bool const is_selected = (edited.subtree_reference == name);
                    if (ImGui::Selectable(name.c_str(), is_selected))
                    {
                        edited.subtree_reference = name;
                    }
                    if (is_selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            // A definition has to exist for the tab to be reachable, so it is
            // created from here rather than by typing a name that leads
            // nowhere.
            ImGui::InputText("##NewSubTreeName", &m_node_edit.subtree_name);
            ImGui::SameLine();
            if (ImGui::Button("New definition"))
            {
                edited.subtree_reference =
                    createSubTreeDefinition(m_node_edit.subtree_name.empty()
                                                ? edited.name
                                                : m_node_edit.subtree_name);
                m_node_edit.subtree_name.clear();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Only show inputs/outputs for nodes that can have blackboard ports
        if (canHaveBlackboardPorts(edited.type))
        {
            if (is_subtree)
            {
                ImGui::TextWrapped(
                    "Port remapping: the left column is the key seen inside "
                    "the subtree, the right one the expression evaluated in "
                    "this tree. Use ${key} to wire a blackboard entry.");
                ImGui::Spacing();
            }

            showPortTable(is_subtree ? "Into the subtree" : "Inputs",
                          edited.inputs,
                          m_node_edit.new_input,
                          is_subtree);

            ImGui::Spacing();

            showPortTable(is_subtree ? "Back to this tree" : "Outputs",
                          edited.outputs,
                          m_node_edit.new_output,
                          is_subtree);

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

        showAttributeTable(edited.attributes, m_node_edit.new_attribute);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Validation info
        if (isDecoratorType(edited.type) && node->children.size() > 1)
        {
            ImGui::TextColored(
                ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                "Warning: Decorators should have exactly 1 child");
        }
        if (nodeCategory(edited.type) == "Leaf" && !node->children.empty())
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
            m_node_edit.new_attribute.clear();
        }

        ImGui::SameLine(0, spacing);

        if (ImGui::Button("Apply", ImVec2(button_width, 0)))
        {
            node->type = edited.type;
            node->name = edited.name;
            node->subtree_reference = edited.subtree_reference;
            node->inputs = edited.inputs;
            node->outputs = edited.outputs;
            node->attributes = edited.attributes;

            // A SubTree node without a reachable definition is a dead end: give
            // it one rather than leaving a node nothing can open.
            if ((node->type == "SubTree") &&
                (m_tree_views.find(node->subtree_reference) ==
                 m_tree_views.end()))
            {
                node->subtree_reference = createSubTreeDefinition(
                    node->subtree_reference.empty() ? node->name
                                                    : node->subtree_reference);
            }

            m_is_modified = true;
            onNodeModified.emit(m_selected_node_id);
            ImGui::CloseCurrentPopup();
            m_node_edit.initialized = false;
            m_node_edit.new_input.clear();
            m_node_edit.new_output.clear();
            m_node_edit.new_attribute.clear();
        }

        ImGui::EndPopup();
    }

    // If popup was closed (e.g., via Escape key), reset temp state
    if (!ImGui::IsPopupOpen("Edit Node") && m_node_edit.initialized)
    {
        m_node_edit.initialized = false;
        m_node_edit.new_input.clear();
        m_node_edit.new_output.clear();
        m_node_edit.new_attribute.clear();
    }
}

// ============================================================================
// Blackboard panel
// ============================================================================

// ----------------------------------------------------------------------------
void Editor::showInheritedKeys()
{
    if (!m_blackboard)
        return;

    std::vector<std::string> keys = m_blackboard->keys();
    std::sort(keys.begin(), keys.end());

    if (keys.empty())
    {
        ImGui::TextDisabled("The parent scope is empty.");
        return;
    }

    if (!ImGui::BeginTable("##inherited",
                           2,
                           ImGuiTableFlags_BordersInnerV |
                               ImGuiTableFlags_RowBg))
    {
        return;
    }

    ImGui::TableSetupColumn("Inherited key",
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (std::string const& key : keys)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(key.c_str());
        ImGui::TableNextColumn();
        ImGui::TextDisabled(
            "%s", bt::Blackboard::displayKey(m_blackboard.get(), key).c_str());
    }

    ImGui::EndTable();
}

// ----------------------------------------------------------------------------
void Editor::drawBlackboardPanel()
{
    if (!m_show_blackboard_panel || !m_blackboard)
        return;

    ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Blackboard", &m_show_blackboard_panel))
    {
        ImGui::End();
        return;
    }

    auto& state = m_blackboard_panel;

    // The tab being edited decides the scope. A subtree owns a child scope, as
    // at runtime: it reads what it does not define from the tree calling it.
    refreshActiveSubTreeScope();

    TreeView& view = getCurrentTreeView();
    bool const nested = view.is_subtree;
    bt::Blackboard::Ptr scope = activeBlackboard();
    if (!scope)
    {
        ImGui::End();
        return;
    }

    if (nested)
    {
        ImGui::TextColored(
            ImVec4(0.7f, 0.6f, 0.9f, 1.0f), "Scope: %s", view.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(nested)");
        ImGui::TextWrapped(
            "A subtree has no blackboard section of its own in the file: what "
            "it sees comes from the port remapping of the SubTree nodes "
            "calling it, plus the keys of the parent scope. Edit the remapping "
            "on the node itself.");
    }
    else
    {
        ImGui::TextColored(
            ImVec4(0.7f, 0.8f, 0.9f, 1.0f), "Scope: %s", view.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(root)");
    }

    ImGui::Separator();

    // Folded by default: the form used to take four lines above the entries the
    // user actually came to read.
    if (!nested && ImGui::CollapsingHeader("New variable"))
    {
        float const field_width =
            std::max(80.0f, ImGui::GetContentRegionAvail().x * 0.4f);

        ImGui::PushItemWidth(field_width);
        ImGui::InputText("Name##NewVar", &state.new_var_name);
        ImGui::Combo("Type##NewVar",
                     &state.new_var_type,
                     c_value_type_names.data(),
                     static_cast<int>(c_value_type_names.size()));

        // Only show value input for non-struct types
        if (state.new_var_type != c_struct_type_index)
        {
            ImGui::InputText("Value##NewVar", &state.new_var_value);
        }
        else
        {
            ImGui::TextDisabled("(created empty, add fields afterwards)");
        }
        ImGui::PopItemWidth();

        ImGui::BeginDisabled(state.new_var_name.empty());
        if (ImGui::Button("Add variable", ImVec2(140.0f, 0.0f)))
        {
            scope->set(
                state.new_var_name,
                makeValueFromText(state.new_var_type, state.new_var_value));

            state.new_var_name.clear();
            state.new_var_value.clear();
            m_is_modified = true;
        }
        ImGui::EndDisabled();

        ImGui::Separator();
    }

    // Display existing variables using the recursive helpers
    BlackboardDrawContext context{state.edit_buffers,
                                  state.add_field_open,
                                  state.add_field_parent_path,
                                  std::string(),
                                  false,
                                  nested};

    std::vector<std::string> keys = scope->keys();
    std::sort(keys.begin(), keys.end());

    if (keys.empty())
    {
        ImGui::TextDisabled(nested ? "Nothing remapped into this scope yet."
                                   : "No variable yet.");
    }
    else if (beginBlackboardTable("##blackboard"))
    {
        for (std::string const& key : keys)
        {
            auto raw_value = scope->raw(key);
            if (!raw_value.has_value())
                continue;

            ImGui::PushID(key.c_str());

            // Make a mutable copy for editing
            BlackboardValue value_copy = *raw_value;

            context.modified = false;
            drawBlackboardRow(key, value_copy, key, context);

            if (context.modified)
            {
                scope->set(key, value_copy);
                m_is_modified = true;
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (!context.remove_request.empty() &&
        (context.remove_request.find('.') == std::string::npos) &&
        (context.remove_request.find('[') == std::string::npos))
    {
        scope->remove(context.remove_request);
        m_is_modified = true;
    }

    if (nested)
    {
        ImGui::Spacing();
        ImGui::Checkbox("Show keys inherited from the parent scope",
                        &state.show_inherited);
        if (state.show_inherited)
        {
            showInheritedKeys();
        }
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

            auto root_value = scope->raw(root_key);
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

                    scope->set(root_key, value_copy);
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
