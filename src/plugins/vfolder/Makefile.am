# Makefile.am for "src/plugins/vfolder" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.

plugindir = $(pkglibdir)/plugins

if BUILD_VFOLDER_PLUGIN
plugin_LTLIBRARIES = vfolder.la
endif

vfolder_la_SOURCES = \
		vfolder.c \
		vfolder_init.c \
		vfolder_gtk.c \
		vfolder_prop.c

nodist_include_HEADERS = \
		vfolder_gtk.h \
		vfolder_prop.h

vfolderincludedir = $(pkgincludedir)/plugins/@PACKAGE@
vfolderinclude_HEADERS = \
			vfolder.h

vfolder_la_LDFLAGS = \
		-avoid-version -module \
		$(GTK_LIBS)

vfolder_la_LIBADD =

vfolder_la_CPPFLAGS = \
		-I$(top_srcdir)/src \
		-I$(top_srcdir)/src/common \
		-I$(top_builddir)/src/common \
		-I$(top_srcdir)/src/gtk \
		$(GLIB_CFLAGS) \
		$(GTK_CFLAGS)

EXTRA_DIST = TODO
