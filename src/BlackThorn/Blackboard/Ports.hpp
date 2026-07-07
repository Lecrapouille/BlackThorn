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
    // ------------------------------------------------------------------------
    //! \brief Construct a port descriptor.
    //! \param[in] p_name Port name declared in YAML.
    //! \param[in] p_direction Input, output, or bidirectional.
    //! \param[in] p_default_value Optional default when unset in YAML.
    // ------------------------------------------------------------------------
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
//!
//! Key features:
//! - Declare typed inputs with optional defaults.
//! - Declare typed outputs.
//! - Query port direction by name at runtime.
// ****************************************************************************
class PortList
{
public:

    // ------------------------------------------------------------------------
    //! \brief Register an input port.
    //! \param[in] p_name Port name.
    //! \param[in] p_default_value Optional default used when YAML omits the port.
    // ------------------------------------------------------------------------
    template <typename T>
    void addInput(std::string const& p_name,
                  std::optional<T> p_default_value = std::nullopt)
    {
        m_inputs[p_name] = PortInfo{typeid(T), p_default_value.has_value()};
    }

    // ------------------------------------------------------------------------
    //! \brief Register an output port.
    //! \param[in] p_name Port name.
    // ------------------------------------------------------------------------
    template <typename T>
    void addOutput(std::string const& p_name)
    {
        m_outputs[p_name] = PortInfo{typeid(T), false};
    }

    // ------------------------------------------------------------------------
    //! \brief Whether \p p_name is a declared input port.
    //! \param[in] p_name Port name to query.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isInput(std::string const& p_name) const;

    // ------------------------------------------------------------------------
    //! \brief Whether \p p_name is a declared output port.
    //! \param[in] p_name Port name to query.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isOutput(std::string const& p_name) const;

private:

    // ------------------------------------------------------------------------
    //! \brief Type-erased port metadata stored in the registry.
    //!
    //! \ref PortList keeps only the declared C++ type and whether a default
    //! value was provided; actual defaults live in \ref Port<T>.
    // ------------------------------------------------------------------------
    struct PortInfo
    {
        PortInfo();
        PortInfo(std::type_index p_type, bool p_has_default);

        //! \brief Declared C++ type of the port (\c typeid(T)).
        std::type_index type;
        //! \brief Whether the port was registered with a default value.
        bool has_default;
    };

    //! \brief Registered input ports keyed by YAML parameter name.
    std::unordered_map<std::string, PortInfo> m_inputs;
    //! \brief Registered output ports keyed by YAML parameter name.
    std::unordered_map<std::string, PortInfo> m_outputs;
};

} // namespace bt
