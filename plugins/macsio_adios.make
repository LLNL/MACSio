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

# This floating point variable is used to order plugin objects during
# the main link for MACSio to allow dependent libraries that are common
# to multiple plugins to be placed later on the link line. Bigger
# numbers here cause them to appear later in the link line.
ADIOS_BUILD_ORDER = 1.0

ADIOS_VERSION = 1.11.0
ADIOS_FILE = adios-$(ADIOS_VERSION).tar.gz
ADIOS_URL = http://users.nccs.gov/~pnorbert/$(ADIOS_FILE)

ifneq ($(ADIOS_HOME),)

ADIOS_LDFLAGS = -L$(ADIOS_HOME)/lib -ladios -Wl,-rpath,$(ADIOS_HOME)/lib
ADIOS_CFLAGS = -I$(ADIOS_HOME)/include

#MXML_HOME =
ADIOS_LDFLAGS += -L$(MXML_HOME)/lib -lmxml -Wl,-rpath,$(MXML_HOME)/lib
MXML_CFLAGS += -I$(MXML_HOME)/include

ADIOS_SOURCES = macsio_adios.c

ADIOS_LDFLAGS += -lz -lm

PLUGIN_OBJECTS += $(ADIOS_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(ADIOS_LDFLAGS)
PLUGIN_LIST += adios

endif

macsio_adios.o: ../plugins/macsio_adios.c
	$(CXX) -c $(ADIOS_CFLAGS) $(MACSIO_CFLAGS) $(CFLAGS) ../plugins/macsio_adios.c

$(ADIOS_FILE):
	$(DLCMD) $(ADIOS_FILE) $(ADIOS_URL)

list-tpls-adios:
	@echo "$(ADIOS_FILE) ($(ADIOS_URL))"

download-tpls-adios: $(ADIOS_FILE)
