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

CFLAGS += -g -DPARALLEL -DHAVE_MPI
CXX ?= mpicxx
CC ?= mpicc
LINK ?= mpicxx

COMMON_SRC = macsio_clargs.c macsio_mif.c macsio_iface.c macsio_timing.c macsio_utils.c macsio_log.c
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
	@echo "    If there is already a config-file in the config-site dir"
	@echo "        for the host you are on, make should find it automatically."
	@echo "    Use target 'all' to build everything."
	@echo "    Use target 'tstlog' to build test for MACSIO_LOG."
	@echo "    Use target 'tsttiming' to build test for MACSIO_TIMING."
	@echo "    Use target 'clean' to clean up objects and executables."
	@echo "    Use target 'dataclean' to clean + remove various data files."
	@echo "    Use target 'allclean' to dataclean + remove json-c install."

all: json-c tsttiming tstlog macsio probe

json-c: ../json-c/install

../json-c/install:
	cd ../json-c; \
	mkdir build install; \
	cd build; \
	../json-c-0.12-20140410b/configure CC="$(CC)" --prefix=`pwd`/../install; \
        make install
	
macsio_timing.o: macsio_timing.h

macsio_main.o: $(BASIC_SRC) $(BASIC_HDR) $(COMMON_HDR)

FOO_OBJ=$(shell make -C ../plugins objlist)
FOO_OBJ2=$(foreach foo, $(FOO_OBJ), ../plugins/$(foo))
FOO_LDFLAGS=$(shell make -C ../plugins ldflags)
macsio: $(BASIC_OBJ) $(COMMON_OBJ) plugins $(FOO_OBJ2)
	$(LINK) $< -o $@ $(FOO_OBJ2) $(COMMON_OBJ) $(DRIVER_LIBS) $(DRIVER_RPATHS) $(FOO_LDFLAGS)

clean:
	rm -f $(DRIVER_OBJ) $(BASIC_OBJ) $(COMMON_OBJ) $(TEST_OBJ) $(TOOL_OBJ)
	rm -f macsio tstopts tsttiming tstlog probe
	$(MAKE) -C ../plugins $@

dataclean: clean
	rm -f *.json *.silo *.log

allclean: dataclean
	rm -rf ../json-c/{install,build}

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

