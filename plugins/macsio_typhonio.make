# Written by James Dickson
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
TYPHONIO_BUILD_ORDER = 3.0

TYPHONIO_VERSION = 1.6.0

ifneq ($(TYPHONIO_HOME),)

TYPHONIO_LDFLAGS = -L$(TYPHONIO_HOME)/lib -ltyphonio -Wl,-rpath,$(TYPHONIO_HOME)/lib
TYPHONIO_CFLAGS = -I$(TYPHONIO_HOME)/include

TYPHONIO_SOURCES = macsio_typhonio.c

ifneq ($(HDF5_HOME),)
TYPHONIO_LDFLAGS += -L$(HDF5_HOME)/lib -lhdf5 -Wl,-rpath,$(HDF5_HOME)/lib
TYPHONIO_CFLAGS += -I$(HDF5_HOME)/include
endif

TYPHONIO_LDFLAGS += -lz -lm

PLUGIN_OBJECTS += $(TYPHONIO_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(TYPHONIO_LDFLAGS)
PLUGIN_LIST += typhonio

endif

CFLAGS += -w

macsio_typhonio.o: ../plugins/macsio_typhonio.c
	$(CXX) -c $(TYPHONIO_CFLAGS) $(MACSIO_CFLAGS) $(CFLAGS) ../plugins/macsio_typhonio.c
