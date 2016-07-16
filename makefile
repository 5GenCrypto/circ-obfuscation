CC 	   = gcc
CFLAGS = -Wall -Wno-unused-result -Wno-pointer-sign -Wno-switch \
		 --std=gnu11 -g \
		 -fopenmp

IFLAGS = -Isrc -Ibuild/include
LFLAGS = -lacirc -lgmp -lm -lclt13 -laesrand -Lbuild/lib -Wl,-rpath -Wl,build/lib

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
