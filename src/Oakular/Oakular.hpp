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
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/BlackThorn
 */

#pragma once

#include "Oakular/Editor.hpp"
#include "Oakular/TreeRenderer.hpp"

#if defined(OAKULAR_HAS_SERVER)
#    include "Oakular/Server.hpp"
#endif
