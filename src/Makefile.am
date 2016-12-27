AUTOMAKE_OPTIONS = foreign -Wall

MY_SOURCES = \
mmap.c \
util.c

AM_CFLAGS = $(EXTRA_CFLAGS) -I$(top_srcdir)
AM_LDFLAGS =

bin_PROGRAMS = circ_obfuscator
circ_obfuscator_SOURCES  =  main.c $(MY_SOURCES)
circ_obfuscator_LDADD = ab/libab.a lin/liblin.a zim/libzim.a
SUBDIRS = ab lin zim
