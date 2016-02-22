#include "circuit.h"

void ensure_space(circuit *c, circref ref);

void circ_init( circuit *c ) {
    c->ninputs = 0;
    c->nconsts = 0;
    c->ngates  = 0;
    c->_size   = 2;
    c->args    = malloc(c->_size * sizeof(*c->args));
    c->ops     = malloc(c->_size * sizeof(*c->ops));
    c->tests   = malloc(sizeof(list));
    list_init(c->tests);
}

void circ_clear( circuit *c ) {
    free(c->args);
    free(c->ops);
    list_clear(c->tests);
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

void circ_add_gate(circuit *c, int ref, operation op, int xref, int yref, bool is_output) {
    ensure_space(c, ref);
    c->ngates   += 1;
    c->ops[ref]  = op;
    c->args[ref] = (int[]) { xref, yref };
}

void ensure_space(circuit *c, circref ref) {
    void *tmp;
    if (ref >= c->_size) {
        tmp = c->args;
        c->args = malloc(2 * c->_size * sizeof(c));
        memcpy(c->args, tmp, c->_size);

        tmp = c->ops;
        c->ops = malloc(2 * c->_size * sizeof(c));
        memcpy(c->ops, tmp, c->_size);

        c->_size *= 2;
        ensure_space(c, ref);
    }
}

operation str2op(char *s) {
    if (strcmp(s, "ADD") == 0) {
        return ADD;
    } else if (strcmp(s, "SUB") == 0) {
        return SUB;
    } else if (strcmp(s, "MUL") == 0) {
        return MUL;
    }
    exit(EXIT_FAILURE);
}

