SILO_BUILD_ORDER = 2.0

ifneq ($(SILO_HOME),)

SILO_SOURCES = macsio_silo.c

PLUGIN_OBJECTS += $(SILO_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(SILO_LDFLAGS)
PLUGIN_LIST += silo

SILO_CFLAGS = -I$(SILO_HOME)/include
SILO_LDFLAGS = -L$(SILO_HOME)/lib -lsilo

ifneq ($(HDF5_HOME),)
HAVE_SILOH5 = $(shell ls $(SILO_HOME)/lib/libsiloh5.{a,so,dylib})
ifneq ($(HAVE_SILOH5),)
SILO_LDFLAGS = -L$(SILO_HOME)/lib -lsiloh5 $(HDF5_LDFLAGS)
else
SILO_USES_HDF5=$(shell nm libsilo.{a,so,dylib} | grep -i h5 | wc -l)
ifneq ($(SILO_USES_HDF5),)
SILO_LDFLAGS += $(HDF5_LDFLAGS)
endif # ifneq ($(SILO_USES_HDF5),)
endif # ifneq ($(HAVE_SILOH5),)
endif # ifneq ($(HDF5_HOME),)

endif

macsio_silo.o: ../plugins/macsio_silo.c
	$(CXX) -c $(SILO_CFLAGS) $(MACSIO_CFLAGS) $(CLFAGS) ../plugins/macsio_silo.c
