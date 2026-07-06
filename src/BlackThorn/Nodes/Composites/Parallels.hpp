/**
 * @file Parallels.hpp
 * @brief Parallel composite nodes: Parallel and ParallelAll.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Nodes/Composites/Composite.hpp"

namespace bt {

// ****************************************************************************
//! \brief The Parallel composite runs all children simultaneously.
//! It requires a minimum number of successful or failed children to determine
//! its own status.
// ****************************************************************************
class Parallel final: public Composite
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "Parallel".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Parallel";
    }

    // ------------------------------------------------------------------------
    //! \brief Constructor taking minimum success and failure counts.
    //! \param[in] p_minSuccess Minimum successful children needed.
    //! \param[in] p_minFail Minimum failed children needed.
    // ------------------------------------------------------------------------
    Parallel(int p_minSuccess, int p_minFail)
        : m_minSuccess(p_minSuccess), m_minFail(p_minFail)
    {
    }

    // ------------------------------------------------------------------------
    //! \brief Copy thresholds into the tree configuration slot.
    //! \param[in,out] p_config Configuration slot to populate.
    // ------------------------------------------------------------------------
    void fillConfig(NodeConfig& p_config) const override
    {
        p_config.parallel_min_success = m_minSuccess;
        p_config.parallel_min_fail = m_minFail;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the minimum number of successful children needed.
    //! \return The minimum number of successful children needed.
    // ------------------------------------------------------------------------
    [[nodiscard]] int getMinSuccess() const
    {
        return tree().config(index()).parallel_min_success;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the minimum number of failed children needed.
    //! \return The minimum number of failed children needed.
    // ------------------------------------------------------------------------
    [[nodiscard]] int getMinFail() const
    {
        return tree().config(index()).parallel_min_fail;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitParallel(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitParallel(*this);
    }

private:

    //! \brief Minimum number of successful children required.
    int m_minSuccess;
    //! \brief Minimum number of failed children required.
    int m_minFail;
};

// ****************************************************************************
//! \brief The ParallelAll composite runs all children simultaneously.
//! Success and failure thresholds can require all children or just one.
// ****************************************************************************
class ParallelAll final: public Composite
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "ParallelAll".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "ParallelAll";
    }

    // ------------------------------------------------------------------------
    //! \brief Constructor configuring all-or-one success/failure semantics.
    //! \param[in] p_successOnAll If true, all children must succeed.
    //! \param[in] p_failOnAll If true, all children must fail.
    // ------------------------------------------------------------------------
    explicit ParallelAll(bool p_successOnAll = true, bool p_failOnAll = true)
        : m_successOnAll(p_successOnAll), m_failOnAll(p_failOnAll)
    {
    }

    // ------------------------------------------------------------------------
    //! \brief Copy flags into the tree configuration slot.
    //! \param[in,out] p_config Configuration slot to populate.
    // ------------------------------------------------------------------------
    void fillConfig(NodeConfig& p_config) const override
    {
        p_config.parallel_success_on_all = m_successOnAll;
        p_config.parallel_fail_on_all = m_failOnAll;
    }

    // ------------------------------------------------------------------------
    //! \brief Return whether all children must succeed.
    //! \return True when success requires every child to succeed.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool getSuccessOnAll() const
    {
        return tree().config(index()).parallel_success_on_all;
    }

    // ------------------------------------------------------------------------
    //! \brief Return whether all children must fail.
    //! \return True when failure requires every child to fail.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool getFailOnAll() const
    {
        return tree().config(index()).parallel_fail_on_all;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitParallelAll(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitParallelAll(*this);
    }

private:

    //! \brief If true, all children must succeed for SUCCESS.
    bool m_successOnAll;
    //! \brief If true, all children must fail for FAILURE.
    bool m_failOnAll;
};

template <> struct NodeKindTraits<Parallel>
{
    static constexpr NodeKind value = NodeKind::Parallel;
};
template <> struct NodeKindTraits<ParallelAll>
{
    static constexpr NodeKind value = NodeKind::ParallelAll;
};

} // namespace bt
