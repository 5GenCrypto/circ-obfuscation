CC 	= gcc
CFLAGS = -Wall --std=c11 -g -O0 -fopenmp #-lrt
IFLAGS = -Isrc
LFLAGS = -lgmp -lm

OBJS = src/clt13.o src/utils.o
SRCS = src/clt13.c src/utils.c 
PARSER = src/parse.tab.c src/scan.yy.c

main: $(OBJS) $(SRCS) $(PARSER) src/parse.tab.h
	$(CC) $(CFLAGS) $(IFLAGS) $(LFLAGS) $(OBJS) $(PARSER) src/main.c -o main

test: $(OBJS) $(SRCS)
	$(CC) $(CFLAGS) $(IFLAGS) $(LFLAGS) $(OBJS) src/test_clt.c -o test

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(IFLAGS) -c -o $@ $<

src/parse.tab.c src/parse.tab.h: src/parse.y
	bison -o src/parse.tab.c -d src/parse.y

src/scan.yy.c: src/scan.l
	flex -o src/scan.yy.c src/scan.l

clean:
	$(RM) $(PARSER)
	$(RM) src/parse.tab.h
	$(RM) src/*.o
	$(RM) test main
	$(RM) circuits/*.acirc.*
