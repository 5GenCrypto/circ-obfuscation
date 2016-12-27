AUTOMAKE_OPTIONS = foreign -Wall

AM_CFLAGS = $(EXTRA_CFLAGS) -I$(top_srcdir)
AM_LDFLAGS = -Wl,--version-script=liblin.version

noinst_LIBRARIES = liblin.a

liblin_a_SOURCES = \
build.c
