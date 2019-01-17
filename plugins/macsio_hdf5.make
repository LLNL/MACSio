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
HDF5_BUILD_ORDER = 1.0

HDF5_VERSION = 1.8.11
HDF5_FILE = hdf5-$(HDF5_VERSION).tar.gz
HDF5_URL = http://www.hdfgroup.org/ftp/HDF5/releases/hdf5-$(HDF5_VERSION)/src/$(HDF5_FILE)

ifneq ($(HDF5_HOME),)

HDF5_LDFLAGS = -L$(HDF5_HOME)/lib -lhdf5 -Wl,-rpath,$(HDF5_HOME)/lib
HDF5_CFLAGS = -I$(HDF5_HOME)/include

HDF5_SOURCES = macsio_hdf5.c

ifneq ($(SZIP_HOME),)
HDF5_LDFLAGS += -L$(SZIP_HOME)/lib -lsz -Wl,-rpath,$(SZIP_HOME)/lib
HDF5_CFLAGS += -DHAVE_SZIP
endif

ifneq ($(ZLIB_HOME),)
HDF5_LDFLAGS += -L$(ZLIB_HOME)/lib
endif

HDF5_LDFLAGS += -lz -lm

PLUGIN_OBJECTS += $(HDF5_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(HDF5_LDFLAGS)
PLUGIN_LIST += hdf5

endif

macsio_hdf5.o: ../plugins/macsio_hdf5.c
	$(CXX) -c $(HDF5_CFLAGS) $(MACSIO_CFLAGS) $(CFLAGS) ../plugins/macsio_hdf5.c

$(HDF5_FILE):
	$(DLCMD) $(HDF5_FILE) $(HDF5_URL)

list-tpls-hdf5:
	@echo "$(HDF5_FILE) ($(HDF5_URL))"

download-tpls-hdf5: $(HDF5_FILE)
