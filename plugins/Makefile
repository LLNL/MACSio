#
# Find a config-site file or use one specified by CONFIG_SITE_FILE
#
HOSTNAME := $(shell hostname -f)
CONFIG_SITE_FILE ?= ../macsio/config-site/$(HOSTNAME)
CONFIG_SITE_FILE_STATUS := $(shell test -e $(CONFIG_SITE_FILE) && echo "not here")
ifeq ($(CONFIG_SITE_FILE_STATUS),)
include config-site/dummy
else
include $(CONFIG_SITE_FILE)
endif

help: list
	@echo "Usage:"
	@echo "    Default will attempt to build all available plugins"
	@echo "    Target 'plugins' also builds all available plugins"
	@echo "    Target 'list' lists all available plugins"
	@echo "    Target 'notes' list to do notes"

list: notes
	@echo
	@echo "List of available plugins is. . ."
	@echo "    $(PLUGIN_LIST)"
	@echo

notes:
	@echo
	@echo "To do..."
	@echo "    Eliminate duplicate inclusion of CONFIG_SITE_FILE"
	@echo "    Get def'n of MACSIO_CFLAGS from main or re-factor it to another .make include file"
	@echo "    Add a clean target"
	@echo "    Add ability to build specific list of plugins"
	@echo

objlist:
	@echo $(PLUGIN_OBJECTS)

# Includes plugins in a specific order, determined by BUILD_ORDER variable in each
# plugin, thereby enabling interdependencies between TPLs used in plugins to be
# more easily and correctly handled.
PLUGIN_INCLUDES = $(shell grep -H '^.*_BUILD_ORDER' ./*.make | sort -n -k 3 | cut -d':' -f1)
include $(PLUGIN_INCLUDES)

MACSIO_CFLAGS = -I../macsio -I../json-c/install/include -DHAVE_MPI

plugins: notes $(PLUGIN_OBJECTS)

clean: notes
	rm -f $(PLUGIN_OBJECTS)
