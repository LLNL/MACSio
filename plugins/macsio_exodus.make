# Exodus plugin depends on Exodus library which may also depend on netCDF and then HDF5
EXODUS_BUILD_ORDER = 3.0

ifneq ($(NETCDF_HOME),)
ifneq ($(EXODUS_HOME),)

EXODUS_SOURCES = macsio_exodus.c

PLUGIN_OBJECTS += $(EXODUS_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(EXODUS_LDFLAGS)
PLUGIN_LIST += exodus

EXODUS_CFLAGS = -I$(EXODUS_HOME)/include -I$(NETCDF_HOME)/include
EXODUS_LDFLAGS = -L$(EXODUS_HOME)/lib -lexodus -L$(NETCDF_HOME)/lib -lnetcdf

NETCDF_USES_HDF5 = $(shell nm $(NETCDF_HOME)/lib/libnetcdf.{a,so,dylib} 2>/dev/null | grep -i h5fopen)

ifneq ($(NETCDF_USES_HDF5),)
EXODUS_LDFLAGS += $(HDF5_LDFLAGS)
endif # ifneq($(NETCDF_USES_HDF5),)

endif # ifneq ($(EXODUS_HOME),)
endif # ifneq ($(NETCDF_HOME),)

macsio_exodus.o: ../plugins/macsio_exodus.c
	$(CXX) -c $(EXODUS_CFLAGS) $(MACSIO_CFLAGS) $(CLFAGS) ../plugins/macsio_exodus.c
