/**
 * @file Ports.cpp
 * @brief Input/output port metadata for behavior tree nodes.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Blackboard/Ports.hpp"

namespace bt {

// ------------------------------------------------------------------------
PortList::PortInfo::PortInfo() : type(typeid(void)), has_default(false) {}

// ------------------------------------------------------------------------
PortList::PortInfo::PortInfo(std::type_index p_type, bool p_has_default)
    : type(p_type), has_default(p_has_default)
{
}

// ------------------------------------------------------------------------
bool PortList::isInput(std::string const& p_name) const
{
    return m_inputs.find(p_name) != m_inputs.end();
}

// ------------------------------------------------------------------------
bool PortList::isOutput(std::string const& p_name) const
{
    return m_outputs.find(p_name) != m_outputs.end();
}

} // namespace bt
