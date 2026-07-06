/**
 * @file Ports.hpp
 * @brief Input/output port metadata for behavior tree nodes.
 *
 * \code
 *   PortList ports;
 *   ports.addInput<int>("speed", 10);
 *   ports.addOutput<std::string>("status");
 *
 *   if (ports.isInput("speed")) { ... }
 * \endcode
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>

namespace bt {

// ****************************************************************************
//! \brief Direction of a node port relative to the blackboard.
// ****************************************************************************
enum class PortDirection
{
    Input,
    Output,
    InOut,
};

// ****************************************************************************
//! \brief Typed port descriptor with optional default value.
// ****************************************************************************
template <typename T>
struct Port
{
    Port(std::string p_name,
         PortDirection p_direction,
         std::optional<T> p_default_value = std::nullopt);

    std::string name;
    PortDirection direction;
    std::optional<T> default_value;
};

template <typename T>
Port<T>::Port(std::string p_name,
              PortDirection p_direction,
              std::optional<T> p_default_value)
    : name(std::move(p_name)),
      direction(p_direction),
      default_value(p_default_value)
{
}

// ****************************************************************************
//! \brief Registry of input and output ports declared by a node type.
// ****************************************************************************
class PortList
{
public:

    template <typename T>
    void addInput(std::string const& p_name,
                  std::optional<T> p_default_value = std::nullopt)
    {
        m_inputs[p_name] = PortInfo{typeid(T), p_default_value.has_value()};
    }

    template <typename T>
    void addOutput(std::string const& p_name)
    {
        m_outputs[p_name] = PortInfo{typeid(T), false};
    }

    [[nodiscard]] bool isInput(std::string const& p_name) const
    {
        return m_inputs.find(p_name) != m_inputs.end();
    }

    [[nodiscard]] bool isOutput(std::string const& p_name) const
    {
        return m_outputs.find(p_name) != m_outputs.end();
    }

private:

    struct PortInfo
    {
        PortInfo();
        PortInfo(std::type_index p_type, bool p_has_default);

        std::type_index type;
        bool has_default;
    };

    std::unordered_map<std::string, PortInfo> m_inputs;
    std::unordered_map<std::string, PortInfo> m_outputs;
};

inline PortList::PortInfo::PortInfo() : type(typeid(void)), has_default(false)
{
}

inline PortList::PortInfo::PortInfo(std::type_index p_type, bool p_has_default)
    : type(p_type), has_default(p_has_default)
{
}

} // namespace bt
