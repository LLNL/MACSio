# This floating point variable is used to order plugin objects during
# the main link for MACSio to allow dependent libraries that are common
# to multiple plugins to be placed later on the link line. Larger 
# numbers here cause them to appear later in the link line relative to
# smaller numbers
MIFTMPL_BUILD_ORDER = 2.0

# Convenience variable to simplify paths to header and library/archive files
# for the I/O library being used by this plugin
MIFTMPL_HOME =

# Convenience variables for any dependent libs (not always needed) for the
# I/O library being used by this plugin
FOO_LIB = /usr/local/foo
BAR_LIB = /etc/bar

# Compiler flags for this plugin to find the I/O library's header files
#MIFTMPL_CFLAGS = -I$(MIFTMPL_HOME)/include
MIFTMPL_CFLAGS =

# Linker flags for this plugin to find the I/O library's lib/archive files
#MIFTMPL_LDFLAGS = -L$(MIFTMPL_HOME)/lib -L$(FOO_LIB) -L$(BAR_LIB) -lmiftmpl -lfoo -lbar
MIFTMPL_LDFLAGS =

# List of source files used by this plugin (usually just one)
MIFTMPL_SOURCES = macsio_miftmpl.c

# Main Makefile variables that this plugin updates
PLUGIN_OBJECTS += $(MIFTMPL_SOURCES:.c=.o)
PLUGIN_LDFLAGS += $(MIFTMPL_LDFLAGS)
PLUGIN_LIST += miftmpl

# Rules to build the object file(s) for this plugin
macsio_miftmpl.o: ../plugins/macsio_miftmpl.c
	$(CXX) -c $(MIFTMPL_CFLAGS) $(MACSIO_CFLAGS) $(CLFAGS) ../plugins/macsio_miftmpl.c
