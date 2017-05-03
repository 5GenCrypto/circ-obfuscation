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

bin_PROGRAMS = mio
mio_SOURCES  =  mio.c $(MY_SOURCES)
mio_LDADD = lz/libobf_lz.la mife/libmife.la mobf/libobf_mobf.la # lin/libobf_lin.la
SUBDIRS = lz mife mobf # lin
