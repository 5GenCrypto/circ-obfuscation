#ifndef __BITVECTOR__
#define __BITVECTOR__

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    int len;
    bool *elem;
} bitvector;

void bv_init( bitvector *bv, int len );
void bv_clear( bitvector *bv );
void bv_from_string( bitvector *bv, char *s );

#endif
