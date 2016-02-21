#ifndef __CIRCUIT_H__
#define __CIRCUIT_H__

#include <stdbool.h>
#include "bitvector.h"
#include "linked-list.h"

typedef struct {
    bitvector *input;
    bool output;
} testcase;

typedef struct {
    list *tests;
} circuit;

void circ_init( circuit *c );

void circ_add_test(circuit *c, bitvector *inp, bool out);

#endif
