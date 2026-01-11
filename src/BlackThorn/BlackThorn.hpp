// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
// Copyright (c) 2025-2026 Quentin Quadrat
//
// This file is part of BlackThorn. It is available under the GNU GPL v3 or,
// for users who cannot use the GPL, under a commercial license.
// See LICENSING.md for details.

/**
 * @file BlackThorn.hpp
 * @brief Main header file for the BlackThorn behavior tree library.
 *
 */

#pragma once

// Core classes
#include "BlackThorn/Core/Composite.hpp"
#include "BlackThorn/Core/Decorator.hpp"
#include "BlackThorn/Core/Leaf.hpp"
#include "BlackThorn/Core/Node.hpp"
#include "BlackThorn/Core/Status.hpp"
#include "BlackThorn/Core/Tree.hpp"

// Blackboard
#include "BlackThorn/Blackboard/Blackboard.hpp"
#include "BlackThorn/Blackboard/Ports.hpp"
#include "BlackThorn/Blackboard/Resolver.hpp"
#include "BlackThorn/Blackboard/Serializer.hpp"

// Builder
#include "BlackThorn/Builder/Builder.hpp"
#include "BlackThorn/Builder/Exporter.hpp"
#include "BlackThorn/Builder/Factory.hpp"

// Composite nodes
#include "BlackThorn/Nodes/Composites/Parallels.hpp"
#include "BlackThorn/Nodes/Composites/Selectors.hpp"
#include "BlackThorn/Nodes/Composites/Sequences.hpp"

// Decorator nodes
#include "BlackThorn/Nodes/Decorators/Logical.hpp"
#include "BlackThorn/Nodes/Decorators/Repeat.hpp"
#include "BlackThorn/Nodes/Decorators/Temporal.hpp"

// Leaf nodes
#include "BlackThorn/Nodes/Leaves/Action.hpp"
#include "BlackThorn/Nodes/Leaves/Basic.hpp"
#include "BlackThorn/Nodes/Leaves/Condition.hpp"
#include "BlackThorn/Nodes/Leaves/SetBlackboard.hpp"
#include "BlackThorn/Nodes/Leaves/Wait.hpp"

// Network
#include "BlackThorn/Network/VisualizerClient.hpp"
