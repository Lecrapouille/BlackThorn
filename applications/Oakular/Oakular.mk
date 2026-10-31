# SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
# Copyright (c) 2025-2026 Quentin Quadrat
#
# This file is part of BlackThorn. See LICENSING.md for details.
#
# Oakular: the behavior tree editor and real-time visualizer. Paths are
# relative to this directory.

TARGETS += Oakular
Oakular.DESCRIPTION := Oakular - Behavior tree editor and real-time visualizer
Oakular.SOURCES := main.cpp OakularApp.cpp Application/*.cpp
Oakular.INCLUDES := .
Oakular.DEPENDS := oakular ImGuiFileDialog
Oakular.CXXFLAGS := $(oakular.CXXFLAGS)
ifeq ($(TARGET_OS),Emscripten)
    ifdef EXAEQUOS
        PKG_CONFIG_STATIC := 1
        Oakular.PKG_CONFIG := exa-wayland glfw
        Oakular.LDFLAGS := -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3
    else
        Oakular.LDFLAGS := -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3 \
            --shell-file $(abspath $(HERE)shell.html) \
            --preload-file $(abspath $(HERE)../../doc/examples)@doc/examples \
            --exclude-file '*.cpp' --exclude-file '*.hpp' --exclude-file '*.md' \
            --exclude-file '*Makefile' --exclude-file '*.mk' \
            -sFORCE_FILESYSTEM=1 -sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -sGL_ENABLE_GET_PROC_ADDRESS
    endif
else
    Oakular.PKG_CONFIG := glfw3 glew gl
endif
