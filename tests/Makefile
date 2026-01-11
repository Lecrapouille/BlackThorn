###############################################################################
## Behavior Tree: A behavior tree library.
## Copyright 2025 Quentin Quadrat <lecrapouille@gmail.com>
##
## This file is part of Behavior Tree.
##
## Behavior Tree is free software: you can redistribute it and/or modify it
## under the terms of the MIT License.
###############################################################################

###################################################
# Location of the project directory and Makefiles
#
P := ..
M := $(P)/.makefile

###################################################
# Project definition
#
include $(P)/Makefile.common
TARGET_NAME := $(PROJECT_NAME)-UnitTest
TARGET_DESCRIPTION := Unit tests for $(PROJECT_NAME)
COMPILATION_MODE = test
include $(M)/project/Makefile

###################################################
# Inform Makefile where to find *.cpp files
#
VPATH += $(P)/tests $(P)/src

###################################################
# Inform Makefile where to find header files
#
INCLUDES += $(P)/tests $(P)/src $(THIRD_PARTIES_DIR)

###################################################
# Make the list of compiled files for the library
#
SRC_FILES += $(call rwildcard,$(P)/src/BlackThorn,*.cpp)
SRC_FILES += $(call rwildcard,$(P)/tests,*.cpp)

###################################################
# Google Test and Google Mock specific warnings
#
USER_CXXFLAGS += -Wno-noexcept

###################################################
# Set Libraries.
#
PKG_LIBS += yaml-cpp sfml-network gtest gmock

###################################################
# Generic Makefile rules
#
include $(M)/rules/Makefile