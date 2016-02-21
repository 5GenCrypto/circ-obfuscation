CC 	= gcc
CFLAGS = -Wall --std=c11 -g -O0 -fopenmp #-lrt
IFLAGS = -Isrc
LFLAGS = -lgmp -lm

LEX=flex
YACC=bison

OBJS = src/clt13.o src/utils.o src/scan.o src/parse.o
SRCS = src/clt13.c src/utils.c 

test.c: $(OBJS) $(SRCS)
	$(CC) $(CFLAGS) $(IFLAGS) $(LFLAGS) $(OBJS) test_clt.c -o test_clt

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(IFLAGS) -c -o $@ $<

src/parse.o: src/parse.y src/parse.c
src/scan.o: src/scan.l

clean:
	rm -f src/scan.c
	rm -f $(OBJS)
	rm -f test_clt
