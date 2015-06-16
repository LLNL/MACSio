# This floating point variable is used to order plugin objects during
# the main link for MACSio to allow dependent libraries that are common
# to multiple plugins to be placed later on the link line. Bigger
# numbers here cause them to appear later in the link line.
HDF5_BUILD_ORDER = 1.0

HDF5_VERSION = 1.8.11
HDF5_FILE = hdf5-$(HDF5_VERSION).tar.gz
HDF5_URL = http://www.hdfgroup.org/ftp/HDF5/releases/hdf5-$(HDF5_VERSION)/src/$(HDF5_FILE)

ifneq ($(HDF5_HOME),)

HDF5_LDFLAGS = -L$(HDF5_HOME)/lib -lhdf5
HDF5_CFLAGS = -I$(HDF5_HOME)/include

HDF5_SOURCES = macsio_hdf5.c

# Lindstrom's ZFP compression library
ifneq ($(ZFP_HOME),)
HDF5_CFLAGS += -DHAVE_ZFP -I$(ZFP_HOME)/include
HDF5_LDFLAGS += -L$(ZFP_HOME)/lib -lzfp
endif

ifneq ($(SZIP_HOME),)
HDF5_LDFLAGS += -L$(SZIP_HOME)/lib -lsz
endif

ifneq ($(ZLIB_HOME),)
HDF5_LDFLAGS += -L$(ZLIB_HOME)/lib
endif

PLUGIN_OBJECTS += $(HDF5_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(HDF5_LDFLAGS)
PLUGIN_LIST += hdf5

HDF5_LDFLAGS += -lz -lm

endif

macsio_hdf5.o: ../plugins/macsio_hdf5.c
	$(CXX) -c $(HDF5_CFLAGS) $(MACSIO_CFLAGS) $(CFLAGS) ../plugins/macsio_hdf5.c

$(HDF5_FILE):
	$(DLCMD) $(HDF5_FILE) $(HDF5_URL)

list-tpls-hdf5:
	@echo "$(HDF5_FILE) ($(HDF5_URL))"

download-tpls-hdf5: $(HDF5_FILE)
