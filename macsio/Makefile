#
# Find a config-site file or use one specified by CONFIG_SITE_FILE
#
HOSTNAME := $(shell hostname -f)
CONFIG_SITE_FILE ?= config-site/$(HOSTNAME)
CONFIG_SITE_FILE_STATUS := $(shell test -e $(CONFIG_SITE_FILE) && echo "not here")
ifeq ($(CONFIG_SITE_FILE_STATUS),)
include config-site/dummy
else
include $(CONFIG_SITE_FILE)
endif

JSON_C_INC=../json-c/install/include
JSON_C_LIB=../json-c/install/lib
JSON_C_LDFLAGS=-ljson-c
DRIVER_INCS += -I$(JSON_C_INC)
DRIVER_LIBS += -L$(JSON_C_LIB) $(JSON_C_LDFLAGS)
DRIVER_RPATHS += -Wl,-rpath,$(JSON_C_LIB)

SILO_INC=$(SILO_HOME)/include
SILO_LIB=$(SILO_HOME)/lib
SILO_LDFLAGS=-lsiloh5 -lz

#  This is for PDB Lite, not to be confused with PDB
PDBL_INC=$(SILO_HOME)/include
PDBL_LIB=$(SILO_HOME)/lib
PDBL_LDFLAGS=-lsiloh5 -lz

HDF5_INC=$(HDF5_HOME)/include
HDF5_LIB=$(HDF5_HOME)/lib
HDF5_LDFLAGS=-lhdf5

# Typically, only for linking w/HDF5

#CFLAGS=-DHAVE_MPI
#CC=mpixlc
#LINK=mpixlcxx

CFLAGS += -g -DPARALLEL -DHAVE_MPI
CXX ?= mpicxx
CC ?= mpicc
LINK ?= mpicxx

COMMON_SRC = macsio_clargs.c macsio_mif.c macsio_params.c options.c ifacemap.c macsio_timing.c macsio_utils.c macsio_log.c
COMMON_HDR=$(COMMON_SRC:.c=.h)
COMMON_OBJ=$(COMMON_SRC:.c=.o)

BASIC_SRC = macsio_main.c
BASIC_HDR=$(BASIC_SRC:.c=.h)
BASIC_OBJ=$(BASIC_SRC:.c=.o)

# Unix stdio and sec2 drivers always present
DRIVER_SRC = ioperf_sec2.c ioperf_stdio.c
CFLAGS += -DHAVE_SEC2 -DHAVE_STDIO

TEST_SRC = tstopts.c tsttiming.c tstlog.c
TEST_OBJ=$(TEST_SRC:.c=.o)

TOOL_SRC = probe.c
TOOL_OBJ=$(TOOL_SRC:.c=.o)

# PDB Lite is part of Silo
# If Silo is present, append it ahead of HDF5
# as it can have dependencies on HDF5
ifeq ($(SILO_HOME),)
else
DRIVER_SRC += macsio_silo.c ioperf_pdbl.c
CFLAGS += -DHAVE_SILO
DRIVER_INCS += -I$(SILO_INC)
DRIVER_LIBS += -L$(SILO_LIB) $(SILO_LDFLAGS)
DRIVER_RPATHS += -Wl,-rpath,$(SILO_LIB)
endif

ifeq ($(HDF5_HOME),)
else
DRIVER_SRC += macsio_hdf5.c
CFLAGS += -DHAVE_HDF5
DRIVER_INCS += -I$(HDF5_INC)
DRIVER_LIBS += -L$(HDF5_LIB) $(HDF5_LDFLAGS)
DRIVER_RPATHS += -Wl,-rpath,$(HDF5_LIB)
ifeq ($(SZIP_HOME),)
else
SZIP_LIB=$(SZIP_HOME)/lib
SZIP_LDFLAGS=-lsz
DRIVER_LIBS += -L$(HDF5_LIB) $(HDF5_LDFLAGS) -L$(SZIP_LIB) $(SZIP_LDFLAGS)
DRIVER_RPATHS += -Wl,-rpath,$(HDF5_LIB) -Wl,-rpath,$(SZIP_LIB)
endif
ifeq ($(ZLIB_HOME),)
else
ZLIB_LIB=$(ZLIB_HOME)/lib
ZLIB_LDFLAGS=-lz
DRIVER_LIBS += -L$(HDF5_LIB) $(HDF5_LDFLAGS) -L$(ZLIB_LIB) $(ZLIB_LDFLAGS)
DRIVER_RPATHS += -Wl,-rpath,$(HDF5_LIB) -Wl,-rpath,$(ZLIB_LIB)
endif
endif

# Don't confuse PDB with PDB Lite (in Silo)
ifeq ($(PDB_HOME),)
else
DRIVER_SRC += ioperf_pdb.c
CFLAGS += -DHAVE_PDB
DRIVER_INCS += -I$(PDB_INC)
DRIVER_LIBS += -L$(PDB_LIB) $(PDB_LDFLAGS)
DRIVER_RPATHS += -Wl,-rpath,$(PDB_LIB)
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
	@echo "    If there is already a config-file in the config-site dir"
	@echo "        for the host you are on, make should find it automatically."
	@echo "    Use target 'all' to build everything."
	@echo "    Use target 'macsio' to build an executable with static plugins."
	@echo "    Use target 'clean' to clean up objects and executables."
	@echo "    Use target 'dataclean' to clean + remove various data files."
	@echo "    Use target 'allclean' to dataclean + remove json-c install."

all: json-c tsttiming tstlog tstopts macsio probe

json-c: ../json-c/install

../json-c/install:
	cd ../json-c; \
	mkdir build install; \
	cd build; \
	../json-c-0.12-20140410b/configure CC="$(CC)" --prefix=`pwd`/../install; \
        make install
	
probe.o: probe.c
probe: probe.o
	$(LINK) $< -o $@ $(LDFLAGS)

tsttiming.o: tsttiming.c $(COMMON_HDR)
tsttiming: tsttiming.o $(COMMON_OBJ)
	$(LINK) $< -o $@ $(COMMON_OBJ) $(LDFLAGS)

tstopts.o: tstopts.c $(COMMON_HDR)
tstopts: tstopts.o $(COMMON_OBJ)
	$(LINK) $< -o $@ $(COMMON_OBJ) $(LDFLAGS)

tstlog.o: tstlog.c $(COMMON_HDR)
tstlog: tstlog.o $(COMMON_OBJ)
	$(LINK) $< -o $@ $(COMMON_OBJ) $(LDFLAGS)

# the PDB Lite driver code is #included from PDB driver code
ioperf_pdbl.o: ioperf_pdb.c

macsio_timing.o: macsio_timing.h

macsio_main.o: $(BASIC_SRC) $(BASIC_HDR) $(COMMON_HDR)

macsio: $(BASIC_OBJ) $(COMMON_OBJ) $(DRIVER_OBJ)
	$(LINK) $< -o $@ $(DRIVER_OBJ) $(COMMON_OBJ) $(DRIVER_LIBS) $(DRIVER_RPATHS) $(LDFLAGS)

clean:
	rm -f $(DRIVER_OBJ) $(BASIC_OBJ) $(COMMON_OBJ) $(TEST_OBJ) $(TOOL_OBJ)
	rm -f macsio tstopts tsttiming tstlog probe

dataclean: clean
	rm -f *.json *.silo *.log

allclean: dataclean
	rm -rf ../json-c/{install,build}

#srun -n2048 -p pdebug macsio --interface silo --parallel_file_mode MIF 5 --avg_num_parts 2.5 --part_size 40000 --part_dim 2 --part_type rectilinear --num_dumps 2 --filebase macsio --fileext silo
check: macsio
	mpirun -np 4 macsio --interface silo --parallel_file_mode MIF 2 --avg_num_parts 2.5 --part_size 40000 --part_dim 2 --part_type rectilinear --num_dumps 2 --filebase macsio --fileext silo
