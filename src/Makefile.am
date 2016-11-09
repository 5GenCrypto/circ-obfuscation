AUTOMAKE_OPTIONS = foreign -Wall

MY_SOURCES = \
evaluate.c \
input_chunker.c \
level.c \
mmap.c \
obf_params.c \
obfuscate.c \
util.c

AM_CFLAGS = $(COMMON_CFLAGS) $(EXTRA_CFLAGS) -I$(top_srcdir)
AM_LDFLAGS = 

bin_PROGRAMS = ab_obfuscator
ab_obfuscator_SOURCES  =  main.c $(MY_SOURCES)
