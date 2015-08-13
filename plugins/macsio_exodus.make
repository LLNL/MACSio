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

# Exodus plugin depends on Exodus library which may also depend on netCDF and then HDF5
EXODUS_BUILD_ORDER = 3.0

NETCDF_VERSION = 4.3.2
EXODUS_VERSION = 6.09

NETCDF_FILE = netcdf-$(NETCDF_VERSION).tar.gz
EXODUS_FILE = exodus-$(EXODUS_VERSION).tar.gz

NETCDF_URL = ftp://ftp.unidata.ucar.edu/pub/netcdf/old/$(NETCDF_FILE)
EXODUS_URL = http://iweb.dl.sourceforge.net/project/exodusii/$(EXODUS_FILE)

ifneq ($(NETCDF_HOME),)
ifneq ($(EXODUS_HOME),)

EXODUS_SOURCES = macsio_exodus.c

EXODUS_CFLAGS = -I$(EXODUS_HOME)/include -I$(NETCDF_HOME)/include
EXODUS_LDFLAGS = -L$(EXODUS_HOME)/lib -lexodus -L$(NETCDF_HOME)/lib -lnetcdf

PLUGIN_OBJECTS += $(EXODUS_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(EXODUS_LDFLAGS)
PLUGIN_LIST += exodus

EXODUS_CFLAGS = -I$(EXODUS_HOME)/include -I$(NETCDF_HOME)/include
EXODUS_LDFLAGS = -L$(EXODUS_HOME)/lib -lexodus -L$(NETCDF_HOME)/lib -lnetcdf

NETCDF_USES_HDF5 = $(shell nm $(NETCDF_HOME)/lib/libnetcdf.{a,so,dylib} 2>/dev/null | grep -i h5fopen)
ifneq ($(NETCDF_USES_HDF5),)
EXODUS_LDFLAGS += $(HDF5_LDFLAGS)
endif # ifneq($(NETCDF_USES_HDF5),)

endif # ifneq ($(EXODUS_HOME),)
endif # ifneq ($(NETCDF_HOME),)

macsio_exodus.o: ../plugins/macsio_exodus.c
	$(CXX) -c $(EXODUS_CFLAGS) $(MACSIO_CFLAGS) $(CFLAGS) ../plugins/macsio_exodus.c

list-tpls-exodus:
	@echo "$(NETCDF_FILE) ($(NETCDF_URL))"
	@echo "$(EXODUS_FILE) ($(EXODUS_URL))"

$(NETCDF_FILE):
	$(DLCMD) $(NETCDF_FILE) $(NETCDF_URL)

$(EXODUS_FILE):
	$(DLCMD) $(EXODUS_FILE) $(EXODUS_URL)

download-tpls-exodus: $(EXODUS_FILE) $(NETCDF_FILE)
