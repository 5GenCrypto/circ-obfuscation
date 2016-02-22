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
    list *tests;
    size_t _size; // alloc size of args and ops
} circuit;

void circ_init( circuit *c );
void circ_clear( circuit *c );

// construction
void circ_add_test(circuit *c, char *inp, char *out);
void circ_add_xinput(circuit *c, int ref, int id);
void circ_add_yinput(circuit *c, int ref, int id, int val);
void circ_add_gate(circuit *c, int ref, operation op, int xref, int yref, bool is_output);

operation str2op(char *s);

#endif
