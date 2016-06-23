#ifndef __SRC_CIRCUIT_H__
#define __SRC_CIRCUIT_H__

#include <gmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef size_t circref;
typedef size_t input_id;

typedef enum {
    XINPUT,
    YINPUT,
    ADD,
    SUB,
    MUL
} operation;

typedef struct {
    size_t ninputs;
    size_t nconsts;
    size_t ngates;
    size_t nrefs;
    size_t ntests;
    size_t noutputs;
    operation *ops;
    circref **args; // [nextref][2]
    circref *outrefs;
    int *consts;
    int **testinps;
    int **testouts;
    size_t _ref_alloc; // alloc size of args and ops
    size_t _test_alloc;
    size_t _outref_alloc;
    size_t _consts_alloc;
} circuit;

void circ_init  (circuit *c);
void circ_clear (circuit *c);

// evaluation
int eval_circ (circuit *c, circref ref, int *xs);
void eval_circ_mod (mpz_t rop, circuit *c, circref ref, mpz_t *xs, mpz_t *ys, mpz_t modulus);
int ensure (circuit *c);

// topological orderings
void topological_order  (int *topo, circuit *c, circref ref);

typedef struct {
    int nlevels;
    int **levels;
    int *level_sizes;
} topo_levels;

topo_levels* topological_levels (circuit *c, circref root);

void topo_levels_destroy (topo_levels *topo);

// info
uint32_t depth  (circuit *c, circref ref);
uint32_t degree (circuit *c, circref ref);

// construction
void circ_add_test   (circuit *c, char *inp, char *out);
void circ_add_xinput (circuit *c, circref ref, size_t input_id);
void circ_add_yinput (circuit *c, circref ref, size_t const_id, int val);
void circ_add_gate   (circuit *c, circref ref, operation op, int xref, int yref, bool is_output);

// helpers
operation str2op (char *s);

#endif
