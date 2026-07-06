/**
 * @file Repeat.hpp
 * @brief Repeat decorator nodes: Repeater, UntilSuccess, UntilFailure.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Nodes/Decorators/Decorator.hpp"

namespace bt {

// ****************************************************************************
//! \brief The Repeater decorator repeats its child node a specified number
//! of times (0 = infinite). Unlike BehaviorTree.CPP, this implementation
//! does not use a while loop; the tree engine handles the tick() calls,
//! allowing proper visualization and reactivity between iterations.
//!
//! The decorator ignores the child's SUCCESS/FAILURE status and continues
//! repeating until the limit is reached.
//!
//! The number of repetitions can be read from the blackboard via port
//! remapping.
// ****************************************************************************
class Repeater final: public Decorator
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "Repeat" (YAML name; \c Repeater is also accepted).
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Repeat";
    }

    // ------------------------------------------------------------------------
    //! \brief Constructor taking a limit of repetitions.
    //! \param[in] p_repetitions The limit of repetitions (0 = infinite).
    // ------------------------------------------------------------------------
    explicit Repeater(size_t p_repetitions = 0)
        : m_default_repetitions(p_repetitions)
    {
    }

    // ------------------------------------------------------------------------
    //! \brief Get the ports provided by the node.
    //! \return The ports provided by the node.
    // ------------------------------------------------------------------------
    [[nodiscard]] PortList providedPorts() const override
    {
        PortList ports;
        ports.addInput<size_t>("repetitions");
        return ports;
    }

    // ------------------------------------------------------------------------
    //! \brief Copy default repetition count into the tree configuration slot.
    //! \param[in,out] p_config Configuration slot to populate.
    // ------------------------------------------------------------------------
    void fillConfig(NodeConfig& p_config) const override
    {
        p_config.repeater_default = m_default_repetitions;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the number of completed repetitions.
    //! \return Current repetition counter from runtime state.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getCount() const
    {
        return tree().runtime().counters[index()];
    }

    // ------------------------------------------------------------------------
    //! \brief Get the configured repetition limit.
    //! \return Maximum number of repetitions (0 = infinite).
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getRepetitions() const
    {
        return tree().runtime().limits[index()];
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitRepeater(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitRepeater(*this);
    }

private:

    //! \brief Default repetition limit when no port remapping is provided.
    size_t m_default_repetitions;
};

//! \brief Backward compatibility alias for \ref Repeater.
using Repeat = Repeater;

// ****************************************************************************
//! \brief The UntilSuccess decorator repeats its child until it succeeds.
//! An optional attempt limit can stop retries with FAILURE.
// ****************************************************************************
class UntilSuccess final: public Decorator
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "UntilSuccess".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "UntilSuccess";
    }

    // ------------------------------------------------------------------------
    //! \brief Constructor taking a maximum number of attempts.
    //! \param[in] p_attempts Maximum attempts (0 = infinite).
    // ------------------------------------------------------------------------
    explicit UntilSuccess(size_t p_attempts = 0) : m_attempts(p_attempts) {}

    // ------------------------------------------------------------------------
    //! \brief Copy attempt limit into the tree configuration slot.
    //! \param[in,out] p_config Configuration slot to populate.
    // ------------------------------------------------------------------------
    void fillConfig(NodeConfig& p_config) const override
    {
        p_config.until_attempts = m_attempts;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the number of completed attempts.
    //! \return Current attempt counter from runtime state.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getCount() const
    {
        return tree().runtime().counters[index()];
    }

    // ------------------------------------------------------------------------
    //! \brief Get the configured attempt limit.
    //! \return Maximum attempts (0 = infinite).
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getAttempts() const
    {
        return tree().config(index()).until_attempts;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitUntilSuccess(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitUntilSuccess(*this);
    }

private:

    //! \brief Maximum number of attempts before returning FAILURE.
    size_t m_attempts;
};

// ****************************************************************************
//! \brief The UntilFailure decorator repeats its child until it fails.
//! An optional attempt limit can stop retries with FAILURE.
// ****************************************************************************
class UntilFailure final: public Decorator
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "UntilFailure".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "UntilFailure";
    }

    // ------------------------------------------------------------------------
    //! \brief Constructor taking a maximum number of attempts.
    //! \param[in] p_attempts Maximum attempts (0 = infinite).
    // ------------------------------------------------------------------------
    explicit UntilFailure(size_t p_attempts = 0) : m_attempts(p_attempts) {}

    // ------------------------------------------------------------------------
    //! \brief Copy attempt limit into the tree configuration slot.
    //! \param[in,out] p_config Configuration slot to populate.
    // ------------------------------------------------------------------------
    void fillConfig(NodeConfig& p_config) const override
    {
        p_config.until_attempts = m_attempts;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the number of completed attempts.
    //! \return Current attempt counter from runtime state.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getCount() const
    {
        return tree().runtime().counters[index()];
    }

    // ------------------------------------------------------------------------
    //! \brief Get the configured attempt limit.
    //! \return Maximum attempts (0 = infinite).
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getAttempts() const
    {
        return tree().config(index()).until_attempts;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitUntilFailure(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitUntilFailure(*this);
    }

private:

    //! \brief Maximum number of attempts before returning FAILURE.
    size_t m_attempts;
};

template <> struct NodeKindTraits<Repeater>
{
    static constexpr NodeKind value = NodeKind::Repeater;
};
template <> struct NodeKindTraits<UntilSuccess>
{
    static constexpr NodeKind value = NodeKind::UntilSuccess;
};
template <> struct NodeKindTraits<UntilFailure>
{
    static constexpr NodeKind value = NodeKind::UntilFailure;
};

} // namespace bt
