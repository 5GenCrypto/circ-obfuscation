AUTOMAKE_OPTIONS = foreign -Wall

MY_SOURCES = \
input_chunker.c \
mmap.c \
util.c

AM_CFLAGS = $(EXTRA_CFLAGS) -I$(top_srcdir)
AM_LDFLAGS =

bin_PROGRAMS = circobf
circobf_SOURCES  =  main.c $(MY_SOURCES)
circobf_LDADD = ab/libab.a lin/liblin.a lz/liblz.a zim/libzim.a
SUBDIRS = ab lin lz zim
