# SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
# Copyright (c) 2025-2026 Quentin Quadrat
#
# This file is part of BlackThorn. See LICENSING.md for details.
#
# Third parties, fetched on the first build. Their commits are locked in
# external/versions.lock: commit it.

TARGETS += rapidyaml imgui ImGuiFileDialog

### A library to parse and emit YAML, and do it fast. License: MIT
rapidyaml.GIT := https://github.com/biojppm/rapidyaml.git
rapidyaml.VERSION := v0.15.2
rapidyaml.SUBMODULES := 1
rapidyaml.TYPE := cmake
rapidyaml.CMAKE_ARGS := -DRYML_BUILD_SHARED_LIBS=OFF -DRYML_BUILD_TESTS=OFF \
    -DRYML_BUILD_BENCHMARKS=OFF -DRYML_BUILD_API=OFF
rapidyaml.LIBRARIES := ryml

### Dear ImGui, with the GLFW and OpenGL 3 backends. License: MIT
imgui.GIT := https://github.com/ocornut/imgui.git
imgui.VERSION := docking
imgui.TYPE := static
imgui.SOURCES := imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp \
    misc/cpp/imgui_stdlib.cpp backends/imgui_impl_glfw.cpp backends/imgui_impl_opengl3.cpp
imgui.PUBLIC_INCLUDES := . misc/cpp backends
ifeq ($(TARGET_OS),Emscripten)
    ifdef EXAEQUOS
        imgui.PKG_CONFIG := glfw
    endif
else
    imgui.PKG_CONFIG := glfw3
endif

### File Dialog for Dear ImGui. License: MIT
ImGuiFileDialog.GIT := https://github.com/aiekick/ImGuiFileDialog.git
ImGuiFileDialog.TYPE := static
ImGuiFileDialog.SOURCES := ImGuiFileDialog.cpp
ImGuiFileDialog.PUBLIC_INCLUDES := .
ImGuiFileDialog.DEPENDS := imgui
