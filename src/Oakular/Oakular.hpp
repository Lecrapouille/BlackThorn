// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
// Copyright (c) 2025-2026 Quentin Quadrat
//
// This file is part of BlackThorn. It is available under the GNU GPL v3 or,
// for users who cannot use the GPL, under a commercial license.
// See LICENSING.md for details.

/**
 * @file Oakular.hpp
 * @brief Main header file for Oakular, the BlackThorn behavior tree editor.
 *
 * Oakular is an embeddable Dear ImGui component: it draws inside the frame owned
 * by the host application and never creates a window, an OpenGL context or an
 * ImGui context. The host provides Dear ImGui (headers and compiled sources,
 * including misc/cpp/imgui_stdlib.cpp) and calls, once per frame:
 *
 * \code
 *   #include "Oakular/Oakular.hpp"
 *
 *   oakular::Editor editor;
 *
 *   editor.update(delta_time);
 *   editor.draw("Behavior Tree");
 * \endcode
 *
 * @see https://github.com/Lecrapouille/BlackThorn
 */

#pragma once

#include "Oakular/Editor.hpp"
#include "Oakular/TreeRenderer.hpp"

#if defined(OAKULAR_HAS_SERVER)
#    include "Oakular/Server.hpp"
#endif
