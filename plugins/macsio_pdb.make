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

PDB_BUILD_ORDER = 2.1

SILO_VERSION = 4.10.2
SILO_FILE = silo-$(SILO_VERSION).tar.gz
SILO_URL = https://wci.llnl.gov/content/assets/docs/simulation/computer-codes/silo/silo-$(SILO_VERSION)/$(SILO_FILE)

ifneq ($(SILO_HOME),)

PDB_SOURCES = macsio_pdb.c

PLUGIN_OBJECTS += $(PDB_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(PDB_LDFLAGS)
PLUGIN_LIST += pdb

PDB_CFLAGS = -I$(SILO_HOME)/include -DHAVE_SILO
PDB_LDFLAGS = -L$(SILO_HOME)/lib -lsiloh5

endif

macsio_pdb.o: ../plugins/macsio_pdb.c
	$(CXX) -c $(PDB_CFLAGS) $(MACSIO_CFLAGS) $(CFLAGS) ../plugins/macsio_pdb.c

list-tpls-pdb:
	@echo "$(SILO_FILE) ($(SILO_URL))"

download-tpls-pdb: 
    $(DLCMD) $(SILO_FILE) $(SILO_URL)
