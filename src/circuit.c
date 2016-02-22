#include "circuit.h"

void circ_init( circuit *c ) {
    c->ninputs = 0;
    c->nconsts = 0;
    c->ngates  = 0;
    c->size  = 2;
    c->args  = malloc(c->size * sizeof(c->args));
    c->ops   = malloc(c->size * sizeof(c->ops));
    c->tests = malloc(sizeof(list));
    list_init(c->tests);
}

void circ_add_test(circuit *c, char *inp, char *out) {
    bitvector tmp;
    bv_init(&tmp, strlen(inp));
    bv_from_string(&tmp, inp);
    testcase t = { &tmp, atoi(out) };
    list_add_node(c->tests, &t);
}

void circ_add_xinput(circuit *c, int ref, int id) {
    ensure_space(c, ref);
    c->ninputs += 1;
    c->ops[ref]  = XINPUT;
    c->args[ref] = (int[]) { id, -1 };
}

void circ_add_yinput(circuit *c, int ref, int id, int val) {
    ensure_space(c, ref);
    c->nconsts  += 1;
    c->ops[ref]  = YINPUT;
    c->args[ref] = (int[]) { id, val };
}

void ensure_space(circuit *c, circref ref) {
    if (ref >= c->size) {
        c->size *= 2;
        realloc(c->args, c->size * sizeof(c->args));
        realloc(c->ops,  c->size * sizeof(c->ops));
        ensure_space(c, ref);
    }
}

