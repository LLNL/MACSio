# This floating point variable is used to order plugin objects during
# the main link for MACSio to allow dependent libraries that are common
# to multiple plugins to be placed later on the link line. Bigger
# numbers here cause them to appear later in the link line.
HDF5_BUILD_ORDER = 1.0

HDF5_HOME = /Users/miller86/visit/visit/hdf5/1.8.11-par/i386-apple-darwin12_gcc-4.2

# Libraries HDF5 may depend on
SZIP_HOME = /Users/miller86/visit/visit/szip/2.1/i386-apple-darwin12_gcc-4.2

HDF5_LDFLAGS = -L$(HDF5_HOME)/lib -L$(SZIP_HOME)/lib -lhdf5 -lsz -lz -lm
HDF5_CFLAGS = -I$(HDF5_HOME)/include

HDF5_SOURCES = macsio_hdf5.c

PLUGIN_OBJECTS += $(HDF5_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(HDF5_LDFLAGS)
PLUGIN_LIST += hdf5

macsio_hdf5.o: ../plugins/macsio_hdf5.c
	$(CXX) -c $(HDF5_CFLAGS) $(MACSIO_CFLAGS) $(CLFAGS) ../plugins/macsio_hdf5.c
