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
# to multiple plugins to be placed later on the link line. Larger 
# numbers here cause them to appear later in the link line relative to
# smaller numbers
MIFTMPL_BUILD_ORDER = 2.0

# Convenience variable to simplify paths to header and library/archive files
# for the I/O library being used by this plugin
MIFTMPL_HOME =

# Convenience variables for any dependent libs (not always needed) for the
# I/O library being used by this plugin
FOO_LIB = /usr/local/foo
BAR_LIB = /etc/bar

# Compiler flags for this plugin to find the I/O library's header files
#MIFTMPL_CFLAGS = -I$(MIFTMPL_HOME)/include
MIFTMPL_CFLAGS =

# Linker flags for this plugin to find the I/O library's lib/archive files
#MIFTMPL_LDFLAGS = -L$(MIFTMPL_HOME)/lib -L$(FOO_LIB) -L$(BAR_LIB) -lmiftmpl -lfoo -lbar
MIFTMPL_LDFLAGS =

# List of source files used by this plugin (usually just one)
MIFTMPL_SOURCES = macsio_miftmpl.c

# Main Makefile variables that this plugin updates
PLUGIN_OBJECTS += $(MIFTMPL_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(MIFTMPL_LDFLAGS)
PLUGIN_LIST += miftmpl

# Rules to build the object file(s) for this plugin
macsio_miftmpl.o: ../plugins/macsio_miftmpl.c
	$(CXX) -c $(MIFTMPL_CFLAGS) $(MACSIO_CFLAGS) $(CFLAGS) ../plugins/macsio_miftmpl.c
