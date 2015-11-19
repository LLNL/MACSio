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
VERSION = 0.9
HOSTNAME := $(shell hostname -f)
CONFIG_SITE_FILE ?= config-site/$(HOSTNAME)
CONFIG_SITE_FILE_STATUS := $(shell test -e $(CONFIG_SITE_FILE) && echo "not here")
ifeq ($(CONFIG_SITE_FILE_STATUS),)
include config-site/dummy
else
include $(CONFIG_SITE_FILE)
endif

#
# Set up doxygen command
#
ifeq ($(DOXCMD),)
DOX_IN_PATH=$(shell which doxygen 2>/dev/null 1>/dev/null; echo $$?)
ifeq ($(DOX_IN_PATH),0)
DOXCMD=$(shell which doxygen)
endif
endif

JSON_C_INC=../json-cwx/install/include
JSON_C_LIB=../json-cwx/install/lib
JSON_C_LDFLAGS=-ljson-cwx
DRIVER_INCS += -I$(JSON_C_INC)
DRIVER_LIBS += -L$(JSON_C_LIB) $(JSON_C_LDFLAGS)
DRIVER_RPATHS += -Wl,-rpath,$(JSON_C_LIB)

CFLAGS += -g -DPARALLEL -DHAVE_MPI
CXX ?= mpicxx
CC ?= mpicc
LINK ?= mpicxx

COMMON_SRC = macsio_clargs.c macsio_mif.c macsio_iface.c macsio_timing.c macsio_utils.c macsio_log.c macsio_data.c
COMMON_HDR=$(COMMON_SRC:.c=.h)
COMMON_OBJ=$(COMMON_SRC:.c=.o)

BASIC_SRC = macsio_main.c
BASIC_HDR=$(BASIC_SRC:.c=.h)
BASIC_OBJ=$(BASIC_SRC:.c=.o)

TEST_SRC = tsttiming.c tstlog.c
TEST_OBJ=$(TEST_SRC:.c=.o)

TOOL_SRC = probe.c
TOOL_OBJ=$(TOOL_SRC:.c=.o)

ifeq ($(SCR_HOME),)
else
CFLAGS += -DHAVE_SCR
SCR_INCS = $(SCR_HOME)/include
SCR_LIBS = $(SCR_HOME)/lib
DRIVER_INCS += -I$(SCR_INCS)
LDFLAGS += -L$(SCR_LIBS) -lscr
DRIVER_RPATHS += -Wl,-rpath,$(SCR_LIBS)
endif

LDFLAGS += -L$(JSON_C_LIB) $(JSON_C_LDFLAGS)

DRIVER_OBJ=$(DRIVER_SRC:.c=.o)

# object rule
# We use C Plus Plus compiler due to the special initialization
# needs of drivers.
.c.o:
	$(CXX) $< -o $@ -c $(CFLAGS) -I. $(DRIVER_INCS) 

help:
	@echo "Usage:"
	@echo "    Make [CONFIG_SITE_FILE=<path-to-config-file>] <target>"
	@echo "    Use target 'all' to build everything."
	@echo "    Use target 'check' run some very simple MACSio tests."
	@echo "    Use target 'tstlog' to build test for MACSIO_LOG."
	@echo "    Use target 'tsttiming' to build test for MACSIO_TIMING."
	@echo "    Use target 'probe' to build a test for installed memory size."
	@echo "    Use target 'json-cwx' to re-build/re-install JSON-C lib."
	@echo "    Use target 'clean' to clean up objects and executables."
	@echo "    Use target 'dataclean' to remove various data files."
	@echo "    Use target 'allclean' to clean + dataclean + remove json-cwx install."
	@echo "    Use target 'dist' generate a distribution tarball"
	@echo "    Use target 'dox' to generate Doxygen documentation in ../docs dir."
	@echo "    You will need a file in config-site dir with variables"
	@echo "        named according to the host on which you are building"
	@echo "        and containing paths to TPLs for plugins."
	@echo "    If there is already a file in the config-site dir"
	@echo "        for the host you are on, make should find it automatically."

all: json-cwx tsttiming tstlog macsio probe

json-cwx: ../json-cwx/build/timestamp

../json-cwx/build/timestamp: ../json-cwx/build ../json-cwx/json-cwx/*.h
	cd ../json-cwx/build; \
        make install; \
        touch timestamp

../json-cwx/build:
	cd ../json-cwx; \
	mkdir build install; \
	cd build; \
	../json-cwx/configure CC="$(CC)" --prefix=`pwd`/../install
	
macsio_timing.o: macsio_timing.h

macsio_main.o: $(BASIC_SRC) $(BASIC_HDR) $(COMMON_HDR)

ifneq ($(ENABLE_PLUGINS),)
PLUGIN_OBJ=$(foreach enpl, $(ENABLE_PLUGINS), macsio_$(enpl).o)
else
PLUGIN_OBJ=$(shell make --no-print-directory -C ../plugins objlist)
endif
PLUGIN_OBJ2=$(foreach obj, $(PLUGIN_OBJ), ../plugins/$(obj))
PLUGIN_LDFLAGS=$(shell make --no-print-directory -C ../plugins ldflags)
macsio: $(BASIC_OBJ) $(COMMON_OBJ) plugins $(PLUGIN_OBJ2)
	$(LINK) $< -o $@ $(PLUGIN_OBJ2) $(COMMON_OBJ) $(DRIVER_LIBS) $(DRIVER_RPATHS) $(PLUGIN_LDFLAGS) $(LDFLAGS)

clean:
	rm -f $(DRIVER_OBJ) $(BASIC_OBJ) $(COMMON_OBJ) $(TEST_OBJ) $(TOOL_OBJ)
	rm -f macsio tstopts tsttiming tstlog probe
	$(MAKE) -C ../plugins $@

# should use plugin's extensions from their individual .make files here
dataclean:
	rm -f *.silo *.h5 *.exoII *.log
	ls *.json | grep -v macsio_json_object.json | xargs rm -f

allclean: clean dataclean doxclean
	rm -rf ../json-cwx/{install,build}

# Standard GNU target
html:	dox

dox:	$(BASIC_SRC) $(BASIC_HDR) $(COMMON_HDR)
	@echo $(DOX_IN_PATH)
	@echo $(DOXCMD)
	$(MAKE) DOXCMD=$(DOXCMD) -C ../doc $@

doxclean:
	rm -rf ../doc/doxyout

dist:	macsio-$(VERSION).tar.gz

macsio-$(VERSION).tar.gz:
	cp -r .. /tmp/macsio-$(VERSION)
	pushd /tmp/macsio-$(VERSION); \
	make allclean; \
	rm -rf .git src-stuff; \
	cd ..; \
	tar cvf - macsio-$(VERSION) | gzip --best > macsio-$(VERSION).tar.gz; \
	popd; \
	mv /tmp/macsio-$(VERSION).tar.gz .; \
	rm -rf /tmp/macsio-$(VERSION)

#srun -n2048 -p pdebug macsio --interface silo --parallel_file_mode MIF 16 --avg_num_parts 2.5 --part_size 40000 --part_dim 2 --part_type rectilinear --num_dumps 2 --filebase macsio --fileext silo
check: macsio
	mpirun -np 4 macsio --interface silo --parallel_file_mode MIF 2 --avg_num_parts 2.5 --part_size 40000 --part_dim 2 --part_type rectilinear --num_dumps 2 --filebase macsio --fileext silo --debug_level 1
	mpirun -np 4 macsio --interface hdf5 --parallel_file_mode MIF 2 --avg_num_parts 2.5 --part_size 40000 --part_dim 2 --part_type rectilinear --num_dumps 2 --filebase macsio --fileext h5 --debug_level 1
	mpirun -np 4 macsio --interface hdf5 --parallel_file_mode SIF 2 --avg_num_parts 2 --part_size 40000 --part_dim 2 --part_type rectilinear --num_dumps 2 --filebase macsio --fileext h5 --debug_level 1
	mpirun -np 4 macsio --interface miftmpl --parallel_file_mode MIF 2 --avg_num_parts 2.5 --part_size 40000 --part_dim 2 --part_type rectilinear --num_dumps 2 --filebase macsio --fileext json --debug_level 1

notes:
	@echo
	@echo "Add support out of source builds"
	@echo

plugins:
	$(MAKE) -C ../plugins $@

#
# Test utility targets
#
probe.o: probe.c
probe: probe.o
	$(LINK) $< -o $@ $(LDFLAGS)

tsttiming.o: tsttiming.c $(COMMON_HDR)
tsttiming: tsttiming.o $(COMMON_OBJ)
	$(LINK) $< -o $@ $(COMMON_OBJ) $(LDFLAGS)

tstlog.o: tstlog.c $(COMMON_HDR)
tstlog: tstlog.o $(COMMON_OBJ)
	$(LINK) $< -o $@ $(COMMON_OBJ) $(LDFLAGS)

