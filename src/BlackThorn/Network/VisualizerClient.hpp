/**
 * @file VisualizerClient.hpp
 * @brief TCP client for real-time behavior tree visualization.
 *
 * This client connects to the Oakular editor in visualizer mode
 * and sends the tree structure (YAML) and state updates (deltas).
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

// Forward declaration for SFML socket
namespace sf {
class TcpSocket;
}

namespace bt {

// Forward declarations
class Tree;

// ****************************************************************************
//! \brief TCP client that sends behavior tree data to the visualizer server.
//!
//! The client connects to the editor running in visualizer mode and:
//! 1. Sends the tree structure as YAML once at connection
//! 2. Sends state changes (deltas) after each tick
//!
//! Usage:
//! \code
//!   bt::Tree tree;
//!   // ... build tree ...
//!
//!   auto visualizer = std::make_shared<bt::VisualizerClient>();
//!   if (visualizer->connect("localhost", 8888)) {
//!       tree.setVisualizerClient(visualizer);
//!   }
//!
//!   // In main loop - tick() automatically sends state changes
//!   while (running) {
//!       tree.tick();
//!   }
//! \endcode
// ****************************************************************************
class VisualizerClient
{
public:

    // ------------------------------------------------------------------------
    //! \brief Constructor.
    // ------------------------------------------------------------------------
    VisualizerClient();

    // ------------------------------------------------------------------------
    //! \brief Destructor - disconnects if connected.
    // ------------------------------------------------------------------------
    ~VisualizerClient();

    // ------------------------------------------------------------------------
    //! \brief Connect to the visualizer server.
    //! \param[in] p_host The server hostname (default: "localhost").
    //! \param[in] p_port The server port (default: 8888).
    //! \return true if connection successful.
    // ------------------------------------------------------------------------
    bool connect(std::string const& p_host = "localhost",
                 uint16_t p_port = 8888);

    // ------------------------------------------------------------------------
    //! \brief Disconnect from the server.
    // ------------------------------------------------------------------------
    void disconnect();

    // ------------------------------------------------------------------------
    //! \brief Check if connected to the server.
    //! \return true if connected.
    // ------------------------------------------------------------------------
    bool isConnected() const;

    // ------------------------------------------------------------------------
    //! \brief Send the tree structure as YAML (called once at connection).
    //! \param[in] p_tree The behavior tree to serialize.
    // ------------------------------------------------------------------------
    void sendTree(Tree const& p_tree);

    // ------------------------------------------------------------------------
    //! \brief Send only the states that have changed since last call.
    //! This is called automatically by Tree::tick() if a visualizer is
    //! attached.
    //! \param[in] p_tree The behavior tree to get states from.
    // ------------------------------------------------------------------------
    void sendStateChanges(Tree const& p_tree);

private:

    // ------------------------------------------------------------------------
    //! \brief Send a string message to the server.
    //! \param[in] p_message The message to send.
    //! \return true if send successful.
    // ------------------------------------------------------------------------
    bool send(std::string const& p_message);

    // ------------------------------------------------------------------------
    //! \brief Serialize the tree to YAML format.
    //! \param[in] p_tree The tree to serialize.
    //! \return The YAML string representation.
    // ------------------------------------------------------------------------
    std::string serializeTreeToYaml(Tree const& p_tree) const;

private:

    //! \brief TCP socket for server communication
    std::unique_ptr<sf::TcpSocket> m_socket;

    //! \brief Flag indicating if the tree has been sent
    bool m_tree_sent = false;

    //! \brief Cache of last sent states for delta detection (node_id -> status
    //! as int)
    std::unordered_map<uint32_t, int> m_last_states;
};

} // namespace bt
