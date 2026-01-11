// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
// Copyright (c) 2025-2026 Quentin Quadrat
//
// This file is part of BlackThorn. It is available under the GNU GPL v3 or,
// for users who cannot use the GPL, under a commercial license.
// See LICENSING.md for details.

/**
 * @file Basic.hpp
 * @brief Basic leaf nodes: Success, Failure.
 *
 */

#pragma once

#include "BlackThorn/Core/Leaf.hpp"

namespace bt {

// ****************************************************************************
//! \brief Simple leaf that always returns SUCCESS.
// ****************************************************************************
class Success final: public Leaf
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "Success".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Success";
    }

    // ------------------------------------------------------------------------
    //! \brief Run the always success leaf.
    //! \return The status of the always success leaf.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status onRunning() override
    {
        return Status::SUCCESS;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitSuccess(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitSuccess(*this);
    }
};

// ****************************************************************************
//! \brief Simple leaf that always returns FAILURE.
// ****************************************************************************
class Failure final: public Leaf
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "Failure".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Failure";
    }

    // ------------------------------------------------------------------------
    //! \brief Run the always failure leaf.
    //! \return The status of the always failure leaf.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status onRunning() override
    {
        return Status::FAILURE;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitFailure(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitFailure(*this);
    }
};

} // namespace bt
