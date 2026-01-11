/**
 * @file Repeat.hpp
 * @brief Repeat decorator nodes: Repeat, UntilSuccess, UntilFailure.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Core/Decorator.hpp"

namespace bt {

// ****************************************************************************
//! \brief The Repeater decorator repeats its child node a specified number
//! of times (0 = infinite). Unlike BehaviorTree.CPP, this implementation
//! does not use a while loop; the tree engine handles the tick() calls,
//! allowing proper visualization and reactivity between iterations.
//! The decorator ignores the child's SUCCESS/FAILURE status and continues
//! repeating until the limit is reached.
// ****************************************************************************
class Repeat final: public Decorator
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "Repeat".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Repeat";
    }

    // ------------------------------------------------------------------------
    //! \brief Constructor taking a limit of repetitions.
    //! \param[in] p_repetitions The limit of repetitions (0 = infinite).
    // ------------------------------------------------------------------------
    explicit Repeat(size_t p_repetitions = 0) : m_repetitions(p_repetitions) {}

    // ------------------------------------------------------------------------
    //! \brief Set up the repeater.
    //! \return The status of the repeater.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status onSetUp() override
    {
        m_count = 0;
        return Status::RUNNING;
    }

    // ------------------------------------------------------------------------
    //! \brief Run the repeater.
    //! \return The status of the repeater.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status onRunning() override
    {
        Status status = m_child->tick();

        if (status == Status::RUNNING)
        {
            return Status::RUNNING;
        }

        // Child finished (SUCCESS or FAILURE), reset for next iteration
        m_child->reset();

        // Increment count and saturate to m_repetitions if limited
        if (m_repetitions > 0)
        {
            ++m_count;
            if (m_count >= m_repetitions)
            {
                m_count = m_repetitions;
                return Status::SUCCESS;
            }
        }

        // Continue (infinite or more iterations needed)
        return Status::RUNNING;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the current count of repetitions.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getCount() const
    {
        return m_count;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the limit number of repetitions.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getRepetitions() const
    {
        return m_repetitions;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitRepeat(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitRepeat(*this);
    }

private:

    size_t m_count = 0;
    size_t m_repetitions;
};

// ****************************************************************************
//! \brief The UntilSuccess decorator repeats until the child returns success
//! and then returns success. Unlike BehaviorTree.CPP, this implementation
//! does not use a while loop; the tree engine handles the tick() calls,
//! allowing proper visualization and reactivity between iterations.
//! The decorator can optionally limit the number of attempts (0 = infinite).
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
    //! \brief Constructor taking an optional limit of attempts.
    //! \param[in] p_attempts The maximum number of attempts (0 = infinite).
    // ------------------------------------------------------------------------
    explicit UntilSuccess(size_t p_attempts = 0) : m_attempts(p_attempts) {}

    // ------------------------------------------------------------------------
    //! \brief Set up the until success decorator.
    //! \return The status of the decorator.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status onSetUp() override
    {
        m_count = 0;
        return Status::RUNNING;
    }

    // ------------------------------------------------------------------------
    //! \brief Run the until success decorator.
    //! \return The status of the decorator.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status onRunning() override
    {
        Status status = m_child->tick();

        if (status == Status::SUCCESS)
        {
            return Status::SUCCESS;
        }
        else if (status == Status::RUNNING)
        {
            return Status::RUNNING;
        }

        // Child failed, reset for next iteration
        m_child->reset();

        // Increment count and check limit if attempts are limited
        if (m_attempts > 0)
        {
            ++m_count;
            if (m_count >= m_attempts)
            {
                m_count = m_attempts; // Saturate
                return Status::FAILURE;
            }
        }

        // Continue (infinite or more attempts needed)
        return Status::RUNNING;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the current count of attempts.
    //! \return The current count of attempts.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getCount() const
    {
        return m_count;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the limit number of attempts.
    //! \return The limit number of attempts (0 = infinite).
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getAttempts() const
    {
        return m_attempts;
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

    size_t m_count = 0;
    size_t m_attempts;
};

// ****************************************************************************
//! \brief The UntilFailure decorator repeats until the child returns failure
//! and then returns success. Unlike BehaviorTree.CPP, this implementation
//! does not use a while loop; the tree engine handles the tick() calls,
//! allowing proper visualization and reactivity between iterations.
//! The decorator can optionally limit the number of attempts (0 = infinite).
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
    //! \brief Constructor taking an optional limit of attempts.
    //! \param[in] p_attempts The maximum number of attempts (0 = infinite).
    // ------------------------------------------------------------------------
    explicit UntilFailure(size_t p_attempts = 0) : m_attempts(p_attempts) {}

    // ------------------------------------------------------------------------
    //! \brief Set up the until failure decorator.
    //! \return The status of the decorator.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status onSetUp() override
    {
        m_count = 0;
        return Status::RUNNING;
    }

    // ------------------------------------------------------------------------
    //! \brief Execute the decorator.
    //! \return The status of the decorator.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status onRunning() override
    {
        Status status = m_child->tick();

        if (status == Status::FAILURE)
        {
            return Status::SUCCESS;
        }
        else if (status == Status::RUNNING)
        {
            return Status::RUNNING;
        }

        // Child succeeded, reset for next iteration
        m_child->reset();

        // Increment count and check limit if attempts are limited
        if (m_attempts > 0)
        {
            ++m_count;
            if (m_count >= m_attempts)
            {
                m_count = m_attempts; // Saturate
                return Status::FAILURE;
            }
        }

        // Continue (infinite or more attempts needed)
        return Status::RUNNING;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the current count of attempts.
    //! \return The current count of attempts.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getCount() const
    {
        return m_count;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the limit number of attempts.
    //! \return The limit number of attempts (0 = infinite).
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getAttempts() const
    {
        return m_attempts;
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

    size_t m_count = 0;
    size_t m_attempts;
};

} // namespace bt
