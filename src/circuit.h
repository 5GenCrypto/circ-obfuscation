#ifndef __CIRCUIT_H__
#define __CIRCUIT_H__

#include <stdbool.h>
#include "bitvector.h"
#include "linked-list.h"

typedef struct {
    bitvector *input;
    bool output;
} testcase;

typedef int circref;

typedef enum {
    XINPUT,
    YINPUT,
    ADD,
    SUB,
    MUL
} operation;

typedef struct {
    int ninputs;
    int nconsts;
    int ngates;
    operation *ops;
    circref **args; // [nextref][2]
    size_t size; // alloc size of args and ops
    list *tests;
} circuit;

void circ_init( circuit *c );

void circ_add_test(circuit *c, char *inp, char *out);
void circ_add_xinput(circuit *c, int ref, int id);
void circ_add_yinput(circuit *c, int ref, int id, int val);

void ensure_space(circuit *c, circref ref);

#endif
