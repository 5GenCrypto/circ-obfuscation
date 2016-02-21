#include "bitvector.h"

void bv_clear( bitvector *bv ) {
    free(bv->elem);
}

void bv_init( bitvector *bv, int len ) {
    bv->len  = len;
    bv->elem = malloc(sizeof(bool) * len);
}

void bv_from_string( bitvector *bv, char *s ) {
    bv->len = strlen(s);
    bv->elem = realloc(bv->elem, bv->len * sizeof(bool));
    bool *ptr = bv->elem;
    while (*s != '\0') {
        *ptr = s[0] == '1';
        ptr++, s++;
    }

    for (int i = 0; i < bv->len; i++)
        printf("%d", bv->elem[i]);
    puts("");
}
