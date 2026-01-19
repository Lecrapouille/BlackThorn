/**
 * @file Node.hpp
 * @brief Base class for all behavior tree nodes.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Blackboard/Blackboard.hpp"
#include "BlackThorn/Blackboard/Ports.hpp"
#include "BlackThorn/Blackboard/Resolver.hpp"
#include "BlackThorn/Core/Status.hpp"
#include "BlackThorn/Visitors/Visitor.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>

namespace bt {

// Forward declarations
class ConstBehaviorTreeVisitor;
class BehaviorTreeVisitor;

// ****************************************************************************
//! \brief Base class for all nodes in the behavior tree.
// ****************************************************************************
class Node
{
public:

    using Ptr = std::unique_ptr<Node>;

    // ------------------------------------------------------------------------
    //! \brief Create a new node of type T.
    //! \param[in] args The arguments to pass to the constructor of T.
    //! \return A unique pointer to the new node.
    // ------------------------------------------------------------------------
    template <typename T, typename... Args>
    [[nodiscard]] static std::unique_ptr<T> create(Args&&... args)
    {
        static_assert(std::is_base_of_v<Node, T>, "T must inherit from Node");
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    // ------------------------------------------------------------------------
    //! \brief Destructor needed because of virtual methods.
    // ------------------------------------------------------------------------
    virtual ~Node() = default;

    // ------------------------------------------------------------------------
    //! \brief Get the ports provided by the node.
    //! \return The ports provided by the node.
    // ------------------------------------------------------------------------
    [[nodiscard]] virtual PortList providedPorts() const
    {
        return PortList{};
    }

    // ------------------------------------------------------------------------
    //! \brief Execute the curent node.
    //! Call the onSetUp() method if the node was not running on previous tick.
    //! Call the onRunning() method if onSetUp() did not return FAILURE.
    //! Call the onTearDown() method if onRunning() did not return RUNNING.
    //! \return The status of the node (SUCCESS, FAILURE, RUNNING).
    // ------------------------------------------------------------------------
    [[nodiscard]] Status tick()
    {
        if (m_status != Status::RUNNING)
        {
            m_status = onSetUp();
        }
        if (m_status != Status::FAILURE)
        {
            m_status = onRunning();
            if (m_status != Status::RUNNING)
            {
                onTearDown(m_status);
            }
        }
        return m_status;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the status of the node.
    //! \return The status of the node.
    // ------------------------------------------------------------------------
    [[nodiscard]] inline Status status() const
    {
        return m_status;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the type of the node.
    //! \return The type of the node.
    // ------------------------------------------------------------------------
    [[nodiscard]] inline std::string const& type() const
    {
        return m_type;
    }

    // ------------------------------------------------------------------------
    //! \brief Reset the status of the node to INVALID_STATUS. This will force
    //! the node to be re-initialized through the onSetUp() method on the
    //! next tick().
    // ------------------------------------------------------------------------
    virtual void reset()
    {
        m_status = Status::INVALID;
    }

    // ------------------------------------------------------------------------
    //! \brief Halt the execution of the node. This will call onHalt() if the
    //! node is currently running, then reset the node status.
    // ------------------------------------------------------------------------
    virtual void halt()
    {
        if (m_status == Status::RUNNING)
        {
            onHalt();
        }
        m_status = Status::INVALID;
    }

    // ------------------------------------------------------------------------
    //! \brief Method invoked by the method onSetUp() of the Tree class to be
    //! sure the whole tree is valid.
    //! \return True if the node is valid, false otherwise.
    // ------------------------------------------------------------------------
    [[nodiscard]] virtual bool isValid() const = 0;

    // ------------------------------------------------------------------------
    //! \brief Accept a const visitor (read-only).
    //! \param[in] p_visitor The visitor to accept.
    // ------------------------------------------------------------------------
    virtual void accept(ConstBehaviorTreeVisitor& p_visitor) const = 0;

    // ------------------------------------------------------------------------
    //! \brief Accept a non-const visitor (read-write).
    //! \param[in] p_visitor The visitor to accept.
    // ------------------------------------------------------------------------
    virtual void accept(BehaviorTreeVisitor& p_visitor) = 0;

protected: // Port management

    // ------------------------------------------------------------------------
    //! \brief Get an input from the port.
    //! \param[in] p_port The port to get the input from.
    //! \param[in] p_bb The blackboard to get the input from.
    //! \return The input value.
    // ------------------------------------------------------------------------
    template <typename T>
    std::optional<T> getInput(std::string const& p_port,
                              Blackboard const& p_bb) const
    {
        auto key = m_port_config.find(p_port) != m_port_config.end()
                       ? m_port_config.at(p_port)
                       : p_port;
        return VariableResolver::resolveValue<T>(key, p_bb);
    }

    // ------------------------------------------------------------------------
    //! \brief Set an output to the port.
    //! \param[in] p_port The port to set the output to.
    //! \param[in] p_value The value to set the output to.
    //! \param[in] p_bb The blackboard to set the output to.
    // ------------------------------------------------------------------------
    template <typename T>
    void setOutput(std::string const& p_port, T&& p_value, Blackboard& p_bb)
    {
        if (m_port_config.find(p_port) != m_port_config.end())
        {
            std::string key = m_port_config.at(p_port);
            // Extract the key from ${key}
            std::regex pattern(R"(\$\{([^}]+)\})");
            std::smatch match;
            if (std::regex_match(key, match, pattern))
            {
                p_bb.set(match[1].str(), std::forward<T>(p_value));
            }
            else
            {
                p_bb.set(key, std::forward<T>(p_value));
            }
        }
    }

    // ------------------------------------------------------------------------
    //! \brief Configure the ports of the node.
    //! \param[in] p_config The configuration to set.
    // ------------------------------------------------------------------------
    void configure(std::unordered_map<std::string, std::string> const& p_config)
    {
        m_port_config = p_config;
    }

protected: // Lifecycle methods

    // ------------------------------------------------------------------------
    //! \brief Method invoked by the method tick(), when transitioning from the
    //! state RUNNING. This is a convenient place to setup the node when needed.
    //! \details By default nothing is done, override to handle the node
    //! startup logic.
    //! \return FAILURE if the node could not be initialized, else return
    //! SUCCESS or RUNNING if the node could be initialized.
    // ------------------------------------------------------------------------
    [[nodiscard]] virtual Status onSetUp()
    {
        return Status::RUNNING;
    }

    // ------------------------------------------------------------------------
    //! \brief Method invoked by the method tick() when the action is already in
    //! the RUNNING state.
    //! \details This method shall be overridden to handle the node running
    //! logic.
    //! \return The status of the node (SUCCESS, FAILURE, RUNNING).
    // ------------------------------------------------------------------------
    [[nodiscard]] virtual Status onRunning() = 0;

    // ------------------------------------------------------------------------
    //! \brief Method invoked by the method tick() when the action is no longer
    //! RUNNING. This is a convenient place for a cleanup the node when needed.
    //! \details By default nothing is done, override to handle the node
    //! cleanup logic.
    //! \param[in] status The status of the node (SUCCESS or FAILURE).
    // ------------------------------------------------------------------------
    virtual void onTearDown(Status)
    {
        // Default implementation does nothing
    }

    // ------------------------------------------------------------------------
    //! \brief Method invoked when halt() is called on a RUNNING node.
    //! \details By default nothing is done, override to handle cleanup when
    //! the node is interrupted.
    // ------------------------------------------------------------------------
    virtual void onHalt()
    {
        // Default implementation does nothing
    }

public:

    //! \brief The name of the node.
    std::string name;

    // ------------------------------------------------------------------------
    //! \brief Get the unique ID of this node.
    //! \return The node's unique ID.
    // ------------------------------------------------------------------------
    [[nodiscard]] uint32_t id() const
    {
        return m_id;
    }

    // ------------------------------------------------------------------------
    //! \brief Set the unique ID of this node.
    //! \param[in] p_id The unique ID to assign.
    // ------------------------------------------------------------------------
    void setId(uint32_t p_id)
    {
        m_id = p_id;
    }

protected:

    //! \brief The unique ID of the node (used for visualization protocol).
    uint32_t m_id = 0;
    //! \brief The type of the node.
    std::string m_type;
    //! \brief The status of the node.
    Status m_status = Status::INVALID;
    //! \brief The port configuration for this node (input/output parameters).
    std::unordered_map<std::string, std::string> m_port_config;
};

} // namespace bt
