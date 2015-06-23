# Exodus plugin depends on Exodus library which may also depend on netCDF and then HDF5
EXODUS_BUILD_ORDER = 3.0

NETCDF_VERSION = 4.3.2
EXODUS_VERSION = 6.09

NETCDF_FILE = netcdf-$(NETCDF_VERSION).tar.gz
EXODUS_FILE = exodus-$(EXODUS_VERSION).tar.gz

NETCDF_URL = ftp://ftp.unidata.ucar.edu/pub/netcdf/old/$(NETCDF_FILE)
EXODUS_URL = http://iweb.dl.sourceforge.net/project/exodusii/$(EXODUS_FILE)

ifneq ($(NETCDF_HOME),)
ifneq ($(EXODUS_HOME),)

EXODUS_SOURCES = macsio_exodus.c

EXODUS_CFLAGS = -I$(EXODUS_HOME)/include -I$(NETCDF_HOME)/include
EXODUS_LDFLAGS = -L$(EXODUS_HOME)/lib -lexodus -L$(NETCDF_HOME)/lib -lnetcdf

PLUGIN_OBJECTS += $(EXODUS_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(EXODUS_LDFLAGS)
PLUGIN_LIST += exodus

NETCDF_USES_HDF5 = $(shell nm $(NETCDF_HOME)/lib/libnetcdf.{a,so,dylib} 2>/dev/null | grep -i h5fopen)

ifneq ($(NETCDF_USES_HDF5),)
EXODUS_LDFLAGS += $(HDF5_LDFLAGS)
endif # ifneq($(NETCDF_USES_HDF5),)

endif # ifneq ($(EXODUS_HOME),)
endif # ifneq ($(NETCDF_HOME),)

macsio_exodus.o: ../plugins/macsio_exodus.c
	$(CXX) -c $(EXODUS_CFLAGS) $(MACSIO_CFLAGS) $(CFLAGS) ../plugins/macsio_exodus.c

list-tpls-exodus:
	@echo "$(NETCDF_FILE) ($(NETCDF_URL))"
	@echo "$(EXODUS_FILE) ($(EXODUS_URL))"

$(NETCDF_FILE):
	$(DLCMD) $(NETCDF_FILE) $(NETCDF_URL)

$(EXODUS_FILE):
	$(DLCMD) $(EXODUS_FILE) $(EXODUS_URL)

download-tpls-exodus: $(EXODUS_FILE) $(NETCDF_FILE)
