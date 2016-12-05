AUTOMAKE_OPTIONS = foreign -Wall

MY_SOURCES = \
input_chunker.c \
mmap.c \
util.c \
ab/level.c \
ab/obf_params.c \
ab/obfuscator.c \
ab/encoding.c \
ab/public_params.c \
ab/secret_params.c

AM_CFLAGS = $(EXTRA_CFLAGS) -I$(top_srcdir)
AM_LDFLAGS = 

bin_PROGRAMS = circ_obfuscator
circ_obfuscator_SOURCES  =  main.c $(MY_SOURCES)
