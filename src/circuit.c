#include "circuit.h"

#include "util.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ensure_space(circuit *c, circref ref);

void circ_init(circuit *c) {
    c->ninputs = 0;
    c->nconsts = 0;
    c->ngates  = 0;
    c->ntests  = 0;
    c->outref  = -1;
    c->nrefs   = 0;
    c->_refalloc  = 2;
    c->_testalloc = 2;
    c->args     = malloc(c->_refalloc * sizeof(int **));
    c->ops      = malloc(c->_refalloc * sizeof(operation));
    c->testinps = malloc(c->_testalloc * sizeof(int **));
    c->testouts = malloc(c->_testalloc * sizeof(int *));
}

void circ_clear(circuit *c) {
    for (int i = 0; i < c->ngates; i++) {
        free(c->args[i]);
    }
    free(c->args);
    free(c->ops);
    for (int i = 0; i < c->ntests; i++) {
        free(c->testinps[i]);
    }
    free(c->testinps);
    free(c->testouts);
}

////////////////////////////////////////////////////////////////////////////////
// circuit evaluation

int eval_circ(circuit *c, circref ref, int *xs) {
    operation op = c->ops[ref];
    switch (op) {
        case XINPUT: return xs[c->args[ref][0]];
        case YINPUT: return c->args[ref][1];
    }
    int xres = eval_circ(c, c->args[ref][0], xs);
    int yres = eval_circ(c, c->args[ref][1], xs);
    switch (op) {
        case ADD: return xres + yres;
        case SUB: return xres - yres;
        case MUL: return xres * yres;
    }
    exit(EXIT_FAILURE); // should never be reached
}

mpz_t* eval_circ_mod(circuit *c, circref ref, mpz_t *xs, mpz_t *ys, mpz_t modulus) {
    mpz_t *res = malloc(sizeof(mpz_t));
    mpz_init(*res);
    operation op = c->ops[ref];
    switch (op) {
        case XINPUT: {
            mpz_set(*res, xs[c->args[ref][0]]);
            return res;
        }
        case YINPUT: {
            mpz_set(*res, ys[c->args[ref][0]]);
            return res;
        }
    }
    mpz_t *xres = eval_circ_mod(c, c->args[ref][0], xs, ys, modulus);
    mpz_t *yres = eval_circ_mod(c, c->args[ref][1], xs, ys, modulus);
    switch (op) {
        case ADD: mpz_add(*res, *xres, *yres);
        case SUB: mpz_sub(*res, *xres, *yres);
        case MUL: mpz_mul(*res, *xres, *yres);
    }
    mpz_mod(*res, *res, modulus);
    mpz_clears(*xres, *yres, NULL);
    free(xres);
    free(yres);
    return res;
}

int ensure(circuit *c) {
    int res;
    bool ok = true;
    for (int i = 0; i < c->ntests; i++) {
        res = eval_circ(c, c->outref, c->testinps[i]);
        if (g_verbose) {
            printf("test %d input=", i);
            for (int j = 0; j < c->ninputs; j++) {
                printf("%d", c->testinps[i][j]);
            }
            printf(" expected=%d got=%d\n", c->testouts[i], res > 0);
        }
        ok = ((res > 0) == c->testouts[i]) && ok;
    }
    return ok;
}

////////////////////////////////////////////////////////////////////////////////
// circuit topological ordering

void topo_helper(int ref, int *topo, int *seen, int *i, circuit *c) {
    if (seen[ref]) return;
    operation op = c->ops[ref];
    if (op == ADD || op == SUB || op == MUL) {
        topo_helper(c->args[ref][0], topo, seen, i, c);
        topo_helper(c->args[ref][1], topo, seen, i, c);
    }
    topo[(*i)++] = ref;
    seen[ref]    = 1;
}

void topological_order(int *topo, circuit *c) {
    int *seen = calloc(c->_refalloc, sizeof(int));
    int i = 0;
    topo_helper(c->outref, topo, seen, &i, c);
    free(seen);
}

// dependencies fills an array with the refs to the subcircuit rooted at ref.
// deps is the target array, i is an index into it.
void dependencies_helper(int *deps, int *seen, int *i, circuit *c, int ref) {
    if (seen[ref]) return;
    operation op = c->ops[ref];
    if (op == XINPUT || op == YINPUT) return;
    // otherwise it's an ADD, SUB, or MUL gate
    int xarg = c->args[ref][0];
    int yarg = c->args[ref][1];
    deps[(*i)++] = xarg;
    deps[(*i)++] = yarg;
    dependencies_helper(deps, seen, i, c, xarg);
    dependencies_helper(deps, seen, i, c, yarg);
    seen[ref] = 1;
}

int dependencies(int *deps, circuit *c, int ref) {
    int *seen = calloc(c->nrefs, sizeof(int));
    int ndeps = 0;
    dependencies_helper(deps, seen, &ndeps, c, ref);
    free(seen);
    return ndeps;
}

// returns number of levels
int topological_levels(int **levels, int *level_sizes, circuit *c) {
    int *topo = calloc(c->nrefs, sizeof(int));
    int *deps = malloc(2 * c->nrefs * sizeof(int));
    for (int i = 0; i < c->nrefs; i++)
        level_sizes[i] = 0;
    int max_level = 0;
    topological_order(topo, c);
    for (int i = 0; i < c->nrefs; i++) {
        int ref = topo[i];
        int ndeps = dependencies(deps, c, ref);
        // find the right level for this ref
        for (int j = 0; j < c->nrefs; j++) {
            bool has_dep = any_in_array(deps, ndeps, levels[j], level_sizes[j]);
            if (has_dep) continue; // try the next level
            // otherwise the ref belongs on this level
            levels[j][level_sizes[j]++] = ref; // push this ref
            if (j > max_level) max_level = j;
            break;
        }
    }
    free(topo);
    free(deps);
    return max_level + 1;
}

////////////////////////////////////////////////////////////////////////////////
// circuit info calculations

int depth(circuit *c, circref ref) {
    operation op = c->ops[ref];
    if (op == XINPUT || op == YINPUT) {
        return 0;
    }
    int xres = depth(c, c->args[ref][0]);
    int yres = depth(c, c->args[ref][1]);
    return max(xres, yres);
}

int xdegree(circuit *c, circref ref, int xid) {
    operation op = c->ops[ref];
    if (op == XINPUT)
        if (c->args[ref][0] == xid)
            return 1;
        else
            return 0;
    else if (op == YINPUT)
        return 0;
    int xres = xdegree(c, c->args[ref][0], xid);
    int yres = xdegree(c, c->args[ref][1], xid);
    if (op == ADD || op == SUB)
        return max(xres, yres);
    else if (op == MUL)
        return xres + yres;
    exit(EXIT_FAILURE);
}

int ydegree(circuit *c, circref ref) {
    operation op = c->ops[ref];
    if (op == XINPUT)
        return 0;
    else if (op == YINPUT)
        return 1;
    int xres = ydegree(c, c->args[ref][0]);
    int yres = ydegree(c, c->args[ref][1]);
    if (op == ADD || op == SUB)
        return max(xres, yres);
    else if (op == MUL)
        return xres + yres;
    exit(EXIT_FAILURE);
}

void type_degree (
    uint32_t *rop,
    circref ref,
    circuit *c,
    size_t nsyms,
    size_t (*input_chunker)(size_t input_num, size_t ninputs, size_t nsyms)
){
    operation op = c->ops[ref];
    if (op == XINPUT) {
        size_t k = input_chunker(c->args[ref][0], c->ninputs, nsyms);
        rop[k] = 1;
        return;
    }
    if (op == YINPUT) {
        rop[nsyms] = 1;
        return;
    }

    uint32_t *xtype = calloc(nsyms+1, sizeof(uint32_t));
    uint32_t *ytype = calloc(nsyms+1, sizeof(uint32_t));

    type_degree(xtype, c->args[ref][0], c, nsyms, input_chunker);
    type_degree(ytype, c->args[ref][1], c, nsyms, input_chunker);

    int types_eq = 1;
    for (size_t i = 0; i < nsyms+1; i++)
        types_eq = types_eq && (xtype[i] == ytype[i]);

    if ((op == ADD || op == SUB) && types_eq) {
        for (size_t i = 0; i < nsyms+1; i++)
            rop[i] = xtype[i];
    } else { // types unequal or op == MUL
        for (size_t i = 0; i < nsyms+1; i++)
            rop[i] = xtype[i] + ytype[i];
    }

    free(xtype);
    free(ytype);
}

////////////////////////////////////////////////////////////////////////////////
// circuit creation

void circ_add_test(circuit *c, char *inpstr, char *out) {
    if (c->ntests >= c->_testalloc) {
        c->testinps = realloc(c->testinps, 2 * c->_testalloc * sizeof(int**));
        c->testouts = realloc(c->testouts, 2 * c->_testalloc * sizeof(int*));
        c->_testalloc *= 2;
        return circ_add_test(c, inpstr, out);
    }

    int len = strlen(inpstr);
    int *inp = malloc(len * sizeof(int));

    for (int i = 0; i < len; i++) {
        if (inpstr[len-1 - i] == '1') {
            inp[i] = 1;
        } else {
            inp[i] = 0;
        }
    }

    c->testinps[c->ntests] = inp;
    c->testouts[c->ntests] = atoi(out);
    c->ntests += 1;
}

void circ_add_xinput(circuit *c, circref ref, size_t id) {
    ensure_space(c, ref);
    c->ninputs += 1;
    c->nrefs   += 1;
    c->ops[ref] = XINPUT;
    circref *args = malloc(2 * sizeof(circref));
    args[0] = id;
    args[1] = -1;
    c->args[ref] = args;
}

void circ_add_yinput(circuit *c, circref ref, size_t id, int val) {
    ensure_space(c, ref);
    c->nconsts  += 1;
    c->nrefs    += 1;
    c->ops[ref]  = YINPUT;
    circref *args = malloc(2 * sizeof(circref));
    args[0] = id;
    args[1] = val;
    c->args[ref] = args;
}

void circ_add_gate(circuit *c, circref ref, operation op, int xref, int yref, bool is_output) {
    ensure_space(c, ref);
    c->ngates   += 1;
    c->nrefs    += 1;
    c->ops[ref]  = op;
    circref *args = malloc(2 * sizeof(circref));
    args[0] = xref;
    args[1] = yref;
    c->args[ref] = args;
    if (is_output) c->outref = ref;
}

////////////////////////////////////////////////////////////////////////////////
// helpers

void ensure_space(circuit *c, circref ref) {
    if (ref >= c->_refalloc) {
        c->args = realloc(c->args, 2 * c->_refalloc * sizeof(int**));
        c->ops  = realloc(c->ops,  2 * c->_refalloc * sizeof(operation));
        c->_refalloc *= 2;
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

