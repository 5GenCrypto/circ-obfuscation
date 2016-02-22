CC 	   = gcc
CFLAGS = -Wall -Wno-unused-result --std=gnu11 -g -O2 \
		 -fopenmp \
		 -DYY_NO_UNPUT=1 -DYY_NO_INPUT=1
IFLAGS = -Isrc
LFLAGS = -lgmp -lm

SRCS   = $(wildcard src/*.c)
OBJS   = $(addsuffix .o, $(basename $(SRCS) $(PARSER)))
HEADS  = $(wildcard src/*.h)
PARSER = src/parse.tab.c src/scan.yy.c

all: main test_mmap

main: $(OBJS) $(SRCS) $(HEADS) main.c
	$(CC) $(CFLAGS) $(IFLAGS) $(LFLAGS) $(OBJS) main.c -o main

test_mmap: $(OBJS) $(SRCS) test_clt.c
	$(CC) $(CFLAGS) $(IFLAGS) $(LFLAGS) $(OBJS) test_clt.c -o test_mmap

src/%.o: src/%.c src/parse.tab.h
	$(CC) $(CFLAGS) $(IFLAGS) -c -o $@ $<

src/parse.tab.c src/parse.tab.h: src/parse.y
	bison -o src/parse.tab.c -d src/parse.y

src/scan.yy.c: src/scan.l
	flex -o src/scan.yy.c src/scan.l

clean:
	$(RM) src/parse.tab.h
	$(RM) src/*.o
	$(RM) test_mmap main
	$(RM) circuits/*.acirc.*
	$(RM) $(PARSER)
	$(RM) $(OBJS)
