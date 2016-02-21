CC 	= gcc
CFLAGS = -Wall --std=gnu11 -g -O2 -fopenmp \
		 -DYY_NO_UNPUT=1 -DYY_NO_INPUT=1 \
		 -Wno-unused-result	
IFLAGS = -Isrc
LFLAGS = -lgmp -lm

OBJS = src/clt13.o src/utils.o src/scan.yy.o src/parse.tab.o \
	   src/bitvector.o src/linked-list.o \
	   src/circuit.o
SRCS = src/clt13.c src/utils.c src/circuit.c src/bitvector.c
HEADS = src/parse.tab.h src/bitvector.h src/circuit.h \
		src/linked-list.h
PARSER = src/parse.tab.c src/scan.yy.c

all: main test_mmap

main: $(OBJS) $(SRCS) $(HEADS) src/main.c
	$(CC) $(CFLAGS) $(IFLAGS) $(LFLAGS) $(OBJS) src/main.c -o main

test_mmap: $(OBJS) $(SRCS) src/test_clt.c
	$(CC) $(CFLAGS) $(IFLAGS) $(LFLAGS) $(OBJS) src/test_clt.c -o test_mmap

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
