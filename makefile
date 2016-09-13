CC 	   = gcc
CFLAGS = -Wall -Wextra -g -std=gnu11

IFLAGS = -Isrc
LFLAGS = -lacirc -lgmp -lflint -lm -lmmap -laesrand

SRCS   = $(wildcard src/*.c)
OBJS   = $(addsuffix .o, $(basename $(SRCS)))
HEADS  = $(wildcard src/*.h)

main: $(OBJS) $(SRCS) $(HEADS) main.c 
	$(CC) $(CFLAGS) $(IFLAGS) $(LFLAGS) $(OBJS) main.c -o main

src/%.o: src/%.c 
	$(CC) $(CFLAGS) $(IFLAGS) -c -o $@ $<

deepclean: clean
	$(RM) -r libaesrand
	$(RM) -r clt13
	$(RM) -r libacirc
	$(RM) -r build
	$(RM) vgcore.*

clean:
	$(RM) src/*.o
	$(RM) -r circuits/*.acirc.*
	$(RM) $(OBJS)
	$(RM) main
