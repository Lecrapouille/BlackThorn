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
CURRENT_DIR := $(P)/src/BlackThorn

###############################################################################
# Project definition
#
include $(P)/Makefile.common
TARGET_NAME := blackthorn
TARGET_DESCRIPTION := Library for BlackThorn Behavior Tree
include $(M)/project/Makefile
include $(CURRENT_DIR)/BlackThorn.deps.mk

###############################################################################
# Inform Makefile where to find header files
#
INCLUDES += $(P)/src/ $(THIRD_PARTIES_DIR)

###############################################################################
# Inform Makefile where to find *.cpp files
#
VPATH +=  $(P)/src/

###############################################################################
# Make the list of files to compile
#
LIB_FILES += $(filter-out $(CURRENT_DIR)/Network/%,$(call rwildcard,$(CURRENT_DIR),*.cpp))
HEADER_FILES := $(call rwildcard,$(CURRENT_DIR),*.hpp)

###############################################################################
# Optional network support. The define and the sfml-network dependency come
# from BlackThorn.deps.mk, shared with the consumers of the library.
#
ifeq ($(BLACKTHORN_WITH_NETWORK),1)
LIB_FILES += $(call rwildcard,$(CURRENT_DIR)/Network,*.cpp)
endif

###############################################################################
# Sharable information between all Makefiles
#
include $(M)/rules/Makefile
