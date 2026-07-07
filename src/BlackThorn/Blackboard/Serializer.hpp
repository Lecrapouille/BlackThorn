/**
 * @file Serializer.hpp
 * @brief Blackboard serialization to/from YAML.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Blackboard/Blackboard.hpp"
#include "BlackThorn/Blackboard/BlackboardValue.hpp"
#include <sstream>
#include <string>

namespace bt {

class YamlNode;

// ****************************************************************************
//! \brief Utility for loading/storing blackboard content from/to YAML.
//!
//! The blackboard serializer is a utility class that is used to load the
//! blackboard content from a YAML node and to store the blackboard content into
//! a YAML node.
//!
//! Key features:
//! - Load the blackboard content from a YAML node.
//! - Store the blackboard content into a YAML node.
//! - Resolve ${var} references in the blackboard content.
// ****************************************************************************
class BlackboardSerializer
{
public:

    // ------------------------------------------------------------------------
    //! \brief Populate a blackboard from a parsed YAML node (rapidyaml).
    //! \param[in,out] p_target Blackboard to populate.
    //! \param[in] p_node YAML map containing key/value pairs.
    //! \param[in] p_reference Optional scope used to resolve ${var} references.
    // ------------------------------------------------------------------------
    static void load(Blackboard& p_target,
                     YamlNode const& p_node,
                     Blackboard const* p_reference = nullptr);

    // ------------------------------------------------------------------------
    //! \brief Convert a YAML node into a blackboard value.
    //! \param[in] p_node Source YAML node.
    //! \param[in] p_scope Optional scope for ${var} resolution.
    //! \return Stored blackboard value.
    // ------------------------------------------------------------------------
    [[nodiscard]] static BlackboardValue
    valueFromNode(YamlNode const& p_node, Blackboard const* p_scope = nullptr);

    // ------------------------------------------------------------------------
    //! \brief Serialize the content of a blackboard into YAML text.
    //! \param[in] p_source Blackboard to serialize.
    //! \return YAML map body (one indentation level), or empty when no data.
    // ------------------------------------------------------------------------
    [[nodiscard]] static std::string dump(Blackboard const& p_source);

private:

    static bool isReference(std::string const& p_literal, std::string& p_key);
    static BlackboardValue toValue(YamlNode const& p_node,
                                   Blackboard const* p_scope);
    static void appendYamlValue(std::ostringstream& p_out,
                                BlackboardValue const& p_value,
                                int p_indent);
};

} // namespace bt
