AUTOMAKE_OPTIONS = foreign -Wall

MY_SOURCES = \
input_chunker.c \
mmap.c \
reflist.c \
util.c

AM_CFLAGS = $(EXTRA_CFLAGS) -I$(top_srcdir)
AM_LDFLAGS =

bin_PROGRAMS = circobf
circobf_SOURCES  =  main.c $(MY_SOURCES)
circobf_LDADD = lin/libobf_lin.la lz/libobf_lz.la
SUBDIRS = ab lin lz zim
