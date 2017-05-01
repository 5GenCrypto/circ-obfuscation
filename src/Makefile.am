AUTOMAKE_OPTIONS = foreign -Wall

MY_SOURCES = \
circ_params.c \
index_set.c \
input_chunker.c \
mmap.c \
reflist.c \
util.c

AM_CFLAGS = $(MY_CFLAGS) -I$(top_srcdir)
AM_LDFLAGS =

bin_PROGRAMS = circobf
circobf_SOURCES  =  circobf.c $(MY_SOURCES)
circobf_LDADD = lz/libobf_lz.la mife/libobf_mife.la # lin/libobf_lin.la
SUBDIRS = lz mife # lin
