#ifndef __OBFUSCATE__
#define __OBFUSCATE__

// encoding_sym represents a kind of encoding
typedef enum { X_, U_, Y_, V_, Z_, W_, C_, S_ } encoding_var;

typedef struct {
    encoding_var var;
    int i1;
    int i2;
    int b1;
    int b2;
} encoding_sym;

// index refers to the underlying mmap index, with F being individual indices
// in the spanning sets S_i
typedef enum { Y, X, Z, W, F } index_var;

typedef struct {
    index_var var;
    int i;
    int b;
    int j;
} index;




#endif
