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

SILO_BUILD_ORDER = 2.0

SILO_VERSION = 4.10.2
SILO_FILE = silo-$(SILO_VERSION).tar.gz 
SILO_URL = https://wci.llnl.gov/content/assets/docs/simulation/computer-codes/silo/silo-$(SILO_VERSION)/$(SILO_FILE)

ifneq ($(SILO_HOME),)

SILO_SOURCES = macsio_silo.c

PLUGIN_OBJECTS += $(SILO_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(SILO_LDFLAGS)
PLUGIN_LIST += silo

SILO_CFLAGS = -I$(SILO_HOME)/include
SILO_LDFLAGS = -L$(SILO_HOME)/lib -lsiloh5 -Wl,-rpath,$(SILO_HOME)/lib

ifneq ($(HDF5_HOME),)
HAVE_SILOH5 = $(shell ls $(SILO_HOME)/lib/libsiloh5.{a,so,dylib} 2>/dev/null)
ifneq ($(HAVE_SILOH5),)
SILO_LDFLAGS = -L$(SILO_HOME)/lib -lsiloh5 $(HDF5_LDFLAGS) -Wl,-rpath,$(SILO_HOME)/lib
else
SILO_USES_HDF5=$(shell nm libsilo.{a,so,dylib} 2>/dev/null | grep -i h5fopen | wc -l)
ifneq ($(SILO_USES_HDF5),)
SILO_LDFLAGS += $(HDF5_LDFLAGS)
endif # ifneq ($(SILO_USES_HDF5),)
endif # ifneq ($(HAVE_SILOH5),)
endif # ifneq ($(HDF5_HOME),)

endif

macsio_silo.o: ../plugins/macsio_silo.c
	$(CXX) -c $(SILO_CFLAGS) $(MACSIO_CFLAGS) $(CFLAGS) ../plugins/macsio_silo.c

list-tpls-silo:
	@echo "$(SILO_FILE) ($(SILO_URL))"

$(SILO_FILE):
	$(DLCMD) $(SILO_FILE) $(SILO_URL)

download-tpls-silo: $(SILO_FILE)
