CC 		= gcc
CCFLAGS = -Wall --std=c11 -g -O0 -fopenmp #-lrt
IFLAGS 	= -I/opt/local/include -Isrc/
LFLAGS 	= -L/opt/local/lib -lgmp -lm

OBJS = src/clt13.o src/utils.o 
SRCS = src/clt13.c src/utils.c 

test.c: $(OBJS) $(SRCS)
	$(CC) $(CCFLAGS) $(IFLAGS) $(LFLAGS) $(OBJS) test_clt.c -o test_clt

src/%.o: src/%.c
	$(CC) $(CCFLAGS) $(IFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS)
	rm -f test_clt
