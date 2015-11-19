include Makefile.am.inc

EXTRA_DIST = README.html README-WIN32.html config.h.win32 doc json-cwx.vcproj
SUBDIRS = . tests

lib_LTLIBRARIES = libjson-cwx.la 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = json-cwx.pc

libjson_cincludedir = $(includedir)/json-cwx
libjson_cinclude_HEADERS = \
	arraylist.h \
	bits.h \
	debug.h \
	json.h \
	json_config.h \
	json_c_version.h \
        json_crc.h \
	json_inttypes.h \
	json_object.h \
	json_object_iterator.h \
	json_object_private.h \
	json_tokener.h \
	json_util.h \
	linkhash.h \
	printbuf.h \
	random_seed.h

#libjsonx_includedir = $(libdir)/json-cwx-@VERSION@
#
#libjsonx_include_HEADERS = \
#	json_config.h

libjson_c_la_LDFLAGS = -version-info 2:1:0 -no-undefined @JSON_BSYMBOLIC_LDFLAGS@

libjson_c_la_SOURCES = \
	arraylist.c \
	debug.c \
	json_c_version.c \
        json_crc.c \
	json_object.c \
	json_object_iterator.c \
	json_tokener.c \
	json_util.c \
	linkhash.c \
	printbuf.c \
	random_seed.c


distclean-local:
	-rm -rf $(testsubdir)
	-rm -rf config.h.in~ Makefile.in aclocal.m4 autom4te.cache/ config.guess config.sub depcomp install-sh ltmain.sh missing
	-rm -f INSTALL test-driver tests/Makefile.in compile

maintainer-clean-local:
	-rm -rf configure

uninstall-local:
	rm -rf "$(DESTDIR)@includedir@/json-cwx"
	rm -f "$(DESTDIR)@includedir@/json"

ANDROID_CFLAGS = -I$(top_srcdir) -DHAVE_CONFIG_H

Android.mk: Makefile.am
	androgenizer -:PROJECT json-cwx \
		-:SHARED libjson-cwx \
		-:TAGS eng debug \
		-:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
		-:SOURCES $(libjson_c_la_SOURCES) $(nodist_libjson_c_la_SOURCES) \
		-:CFLAGS $(DEFS) $(ANDROID_CFLAGS) $(libjson_c_la_CFLAGS) \
		-:LDFLAGS $(libjson_c_la_LDFLAGS) $(libjson_c_la_LIBADD) \
		-:HEADER_TARGET json-cwx \
		-:HEADERS $(libjson_cinclude_HEADERS) \
		-:PASSTHROUGH LOCAL_ARM_MODE:=arm \
	> $@
