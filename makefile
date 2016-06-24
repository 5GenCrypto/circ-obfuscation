CC 	   = gcc
CFLAGS = -Wall -Wno-unused-result -Wno-pointer-sign -Wno-switch \
		 --std=gnu11 -g \
		 #-fopenmp \
		 -DYY_NO_UNPUT=1 -DYY_NO_INPUT=1

IFLAGS = -Isrc -Isrc/parser -Iclt13/src
LFLAGS = -lgmp -lm

SRCS   = $(wildcard src/*.c) $(wildcard clt13/src/*.c)
OBJS   = $(addsuffix .o, $(basename $(SRCS)))
HEADS  = $(wildcard src/*.h) $(wildcart clt13/src/*.h)
PARSER = src/parser/parse.tab.c src/parser/scan.yy.c
POBJS  = $(addsuffix .o, $(basename $(PARSER)))

main: clt13 $(OBJS) $(POBJS) $(SRCS) $(HEADS) src/parser/parse.tab.h main.c 
	$(CC) $(CFLAGS) $(IFLAGS) $(LFLAGS) $(OBJS) $(POBJS) main.c -o main

src/%.o: src/%.c 
	$(CC) $(CFLAGS) $(IFLAGS) -c -o $@ $<

src/parser/parse.tab.c src/parser/parse.tab.h: src/parser/parse.y
	bison -o src/parser/parse.tab.c -d src/parser/parse.y

src/parser/scan.yy.c: src/parser/scan.l
	flex -o src/parser/scan.yy.c src/parser/scan.l

clt13: 
	git clone https://github.com/spaceships/clt13.git

clean:
	$(RM) src/parser/parse.tab.h
	$(RM) src/*.o src/parser/*.o
	$(RM) test_mmap main
	$(RM) -r circuits/*.acirc.*
	$(RM) $(PARSER)
	$(RM) $(OBJS)
