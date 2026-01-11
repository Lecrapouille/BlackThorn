###############################################################################
## BlackThorn: A behavior tree library.
## Copyright 2025 Quentin Quadrat <lecrapouille@gmail.com>
##
## This file is part of BlackThorn.
##
## BlackThorn is free software: you can redistribute it and/or modify it
## under the terms of the MIT License.
###############################################################################

###############################################################################
# Location of the project directory and Makefiles
#
P := ../..
M := $(P)/.makefile

###############################################################################
# Project definition
#
include $(P)/Makefile.common
TARGET_NAME := blackthorn
TARGET_DESCRIPTION := Library for BlackThorn Behavior Tree
include $(M)/project/Makefile

###############################################################################
# Inform Makefile where to find *.cpp files
CURRENT_DIR := $(P)/src/BlackThorn

###############################################################################
# Inform Makefile where to find header files
#
INCLUDES += $(P)/src/ $(THIRD_PARTIES_DIR)

###############################################################################
# Inform Makefile where to find *.cpp files
#
VPATH +=  $(P)/src/

###############################################################################
# Project defines
#
DEFINES +=

###############################################################################
# Make the list of files to compile
#
LIB_FILES += $(call rwildcard,$(CURRENT_DIR),*.cpp)
HEADER_FILES := $(call rwildcard,$(CURRENT_DIR),*.hpp)

###############################################################################
# Set Libraries
#
PKG_LIBS += yaml-cpp sfml-network

###############################################################################
# Sharable information between all Makefiles
#
include $(M)/rules/Makefile