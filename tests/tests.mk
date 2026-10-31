# SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
# Copyright (c) 2025-2026 Quentin Quadrat
#
# This file is part of BlackThorn. See LICENSING.md for details.
#
# Unit tests, with Google Test. Paths are relative to this directory.

TARGETS += BlackThorn-UnitTest
BlackThorn-UnitTest.TYPE := test
BlackThorn-UnitTest.DESCRIPTION := Unit tests for BlackThorn
BlackThorn-UnitTest.SOURCES := **/*.cpp
BlackThorn-UnitTest.INCLUDES := .
BlackThorn-UnitTest.DEPENDS := blackthorn
BlackThorn-UnitTest.CXXFLAGS := -Wno-noexcept
BlackThorn-UnitTest.PKG_CONFIG := gtest gmock
