#ifndef __CIRCUIT_H__
#define __CIRCUIT_H__

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "util.h"

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
    int nrefs;
    int ntests;
    circref outref;
    operation *ops;
    circref **args; // [nextref][2]
    int **testinps;
    int *testouts;
    size_t _refalloc; // alloc size of args and ops
    size_t _testalloc;
} circuit;

void circ_init(circuit *c);
void circ_clear(circuit *c);

int eval_circ(circuit *c, circref ref, int *xs);
mpz_t* eval_circ_mod(circuit *c, circref ref, mpz_t *xs, mpz_t *ys, mpz_t modulus);
int ensure(circuit *c);

void topological_order(int *refs, circuit *c);
int topological_levels(int **levels, int *level_sizes, circuit *c);

int depth(circuit *c, circref ref);
int xdegree(circuit *c, circref ref, int xid);
int ydegree(circuit *c, circref ref);

// construction
void circ_add_test(circuit *c, char *inp, char *out);
void circ_add_xinput(circuit *c, int ref, int id);
void circ_add_yinput(circuit *c, int ref, int id, int val);
void circ_add_gate(circuit *c, int ref, operation op, int xref, int yref, bool is_output);

operation str2op(char *s);

#endif
