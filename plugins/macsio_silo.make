SILO_BUILD_ORDER = 2.0
SILO_HOME = /Users/miller86/visit/visit/silo/4.10.2-h5par/i386-apple-darwin12_gcc-4.2

# Libraries Silo may depend on depends on
HDF5_LIB=/Users/miller86/visit/visit/hdf5/1.8.11-par/i386-apple-darwin12_gcc-4.2/lib
SZIP_LIB=/Users/miller86/visit/visit/szip/2.1/i386-apple-darwin12_gcc-4.2/lib

SILO_LDFLAGS = -L$(SILO_HOME)/lib -L$(HDF5_LIB) -L$(SZIP_LIB) -lsiloh5 -lhdf5 -lsz -lz -lm
SILO_CFLAGS = -I$(SILO_HOME)/include

SILO_SOURCES = macsio_silo.c

PLUGIN_OBJECTS += $(SILO_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(SILO_LDFLAGS)
PLUGIN_LIST += silo

macsio_silo.o: ../plugins/macsio_silo.c
	$(CXX) -c $(SILO_CFLAGS) $(MACSIO_CFLAGS) $(CLFAGS) ../plugins/macsio_silo.c
