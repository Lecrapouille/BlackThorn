###############################################################################
## Behavior Tree: A behavior tree library.
## Copyright 2025 Quentin Quadrat <lecrapouille@gmail.com>
##
## This file is part of Behavior Tree.
##
## Behavior Tree is free software: you can redistribute it and/or modify it
## under the terms of the MIT License.
###############################################################################

###############################################################################
# Location of the project directory and Makefiles
#
P := .
M := $(P)/.makefile

###############################################################################
# Project definition
#
include $(P)/Makefile.common
TARGET_NAME := $(PROJECT_NAME)
TARGET_DESCRIPTION := Library for Behavior Tree
ORCHESTRATOR_MODE := 1

include $(M)/project/Makefile

###################################################
# Internal libs to compile in the correct order
#
LIB_BEHAVIOR_TREE := $(call internal-lib,behavior-tree)
INTERNAL_LIBS := $(LIB_BEHAVIOR_TREE)
PATH_SRC_BLACKTHORN := $(P)/src/BlackThorn
PATH_SRC_OAKULAR := $(P)/src/Oakular
DIRS_WITH_MAKEFILE := $(PATH_SRC_BLACKTHORN) $(PATH_SRC_OAKULAR)
$(PATH_SRC_OAKULAR): $(PATH_SRC_BLACKTHORN)

###################################################
# Generic Makefile rules
#
include $(M)/rules/Makefile

###################################################
# Extra rules: compile applications after everything
#
APPLICATIONS = $(PATH_SRC_OAKULAR)/.
EXAMPLES = $(sort $(dir $(wildcard $(P)/doc/examples/*/.)))

.PHONY: applications
applications: $(DIRS_WITH_MAKEFILE)
	@$(call print-from,"Compiling applications",$(PROJECT_NAME),$(APPLICATIONS))
	@for i in $(APPLICATIONS);     \
	do                             \
		$(MAKE) -C $$i all;        \
	done;

.PHONY: examples
examples: $(DIRS_WITH_MAKEFILE)
	@$(call print-from,"Compiling examples",$(PROJECT_NAME),$(EXAMPLES))
	@for i in $(EXAMPLES);     \
	do                          \
		$(MAKE) -C $$i all;     \
	done;

post-build:: applications examples