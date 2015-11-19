# Copyright (c) 2015, Lawrence Livermore National Security, LLC.
# Produced at the Lawrence Livermore National Laboratory.
# Written by Mark C. Miller
# 
# LLNL-CODE-676051. All rights reserved. 
# 
# This file is part of MACSio
# 
# Please also read the LICENSE file at the top of the source code directory or
# folder hierarchy.
# 
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License (as published by the Free Software
# Foundation) version 2, dated June 1991.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT 
# ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General
# Public License for more details.
# 
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 Temple
# Place, Suite 330, Boston, MA 02111-1307 USA

#
# Find a config-site file or use one specified by CONFIG_SITE_FILE
#
HOSTNAME := $(shell hostname -f)
CONFIG_SITE_FILE ?= ../macsio/config-site/$(HOSTNAME)
CONFIG_SITE_FILE_STATUS := $(shell test -e $(CONFIG_SITE_FILE) && echo "not here")
ifeq ($(CONFIG_SITE_FILE_STATUS),)
include config-site/dummy
else
include $(CONFIG_SITE_FILE)
endif

# Set the download command so that '$(DLCMD) file url' works
ifeq ($(OSTYPE),darwin)
DLCMD = curl -o
else
DLCMD = wget -O
endif

help: list
	@echo "Usage:"
	@echo "    Default will attempt to build all available plugins"
	@echo "    Target 'plugins' also builds all available plugins"
	@echo "    Target 'list' lists all available plugins"
	@echo "    Target 'list-tpls-X' lists all TPL(s) needed for plugin X"
	@echo "    Target 'download-tpls-X' downloads all TPL(s) needed for plugin X"
	@echo "    Target 'notes' list to do notes"

list: notes
	@echo
	@echo "List of available plugins is. . ."
	@echo "    $(PLUGIN_LIST)"
	@echo

notes:
	@echo
	@echo "To do..."
	@echo "    Eliminate duplicate inclusion of CONFIG_SITE_FILE"
	@echo "    Get def'n of MACSIO_CFLAGS from main or re-factor it to another .make include file"
	@echo "    Add a clean target"
	@echo "    Add ability to build specific list of plugins"
	@echo

objlist:
	@echo $(PLUGIN_OBJECTS)

ldflags:
	@echo $(PLUGIN_LDFLAGS)

# Includes plugins in a specific order, determined by BUILD_ORDER variable in each
# plugin, thereby enabling interdependencies between TPLs used in plugins to be
# more easily and correctly handled.
PLUGIN_INCLUDES = $(shell grep -H '^.*_BUILD_ORDER' ./*.make | sort -n -k 3 | cut -d':' -f1)
include $(PLUGIN_INCLUDES)

MACSIO_CFLAGS = -I../macsio -I../json-cwx/install/include -DHAVE_MPI

plugins: notes $(PLUGIN_OBJECTS)

clean: notes
	rm -f $(PLUGIN_OBJECTS)
