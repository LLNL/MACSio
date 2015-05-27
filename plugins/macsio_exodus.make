# Exodus plugin depends on Exodus library which may also depend on netCDF and then HDF5
EXODUS_BUILD_ORDER = 3.0

EXODUS_LDFLAGS = -L$(EXODUS_HOME)/lib -lexodus -L$(NETCDF_HOME)/lib -lnetcdf
EXODUS_CFLAGS = -I$(EXODUS_HOME)/include -I$(NETCDF_HOME)/include

EXODUS_SOURCES = macsio_exodus.c

PLUGIN_OBJECTS += $(EXODUS_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(EXODUS_LDFLAGS)
PLUGIN_LIST += exodus

macsio_exodus.o: ../plugins/macsio_exodus.c
	$(CXX) -c $(EXODUS_CFLAGS) $(MACSIO_CFLAGS) $(CLFAGS) ../plugins/macsio_exodus.c
