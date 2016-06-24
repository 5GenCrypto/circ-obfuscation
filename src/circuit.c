#include "circuit.h"

#include "util.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void ensure_gate_space (circuit *c, circref ref);

void circ_init (circuit *c)
{
    c->ninputs  = 0;
    c->nconsts  = 0;
    c->ngates   = 0;
    c->ntests   = 0;
    c->nrefs    = 0;
    c->noutputs = 0;
    c->_ref_alloc    = 2;
    c->_test_alloc   = 2;
    c->_outref_alloc = 2;
    c->_consts_alloc = 2;
    c->outrefs  = lin_malloc(c->_outref_alloc * sizeof(circref));
    c->args     = lin_malloc(c->_ref_alloc    * sizeof(circref*));
    c->ops      = lin_malloc(c->_ref_alloc    * sizeof(operation));
    c->testinps = lin_malloc(c->_test_alloc   * sizeof(int*));
    c->testouts = lin_malloc(c->_test_alloc   * sizeof(int*));
    c->consts   = lin_malloc(c->_consts_alloc * sizeof(int));
}

void circ_clear (circuit *c)
{
    for (int i = 0; i < c->nrefs; i++) {
        free(c->args[i]);
    }
    free(c->args);
    free(c->ops);
    free(c->outrefs);
    for (int i = 0; i < c->ntests; i++) {
        free(c->testinps[i]);
        free(c->testouts[i]);
    }
    free(c->testinps);
    free(c->testouts);
    free(c->consts);
}

////////////////////////////////////////////////////////////////////////////////
// circuit evaluation

int eval_circ (circuit *c, circref ref, int *xs)
{
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

void eval_circ_mod (mpz_t rop, circuit *c, circref ref, mpz_t *xs, mpz_t *ys, mpz_t modulus)
{
    operation op = c->ops[ref];
    switch (op) {
        case XINPUT: {
            mpz_set(rop, xs[c->args[ref][0]]);
            return;
        }
        case YINPUT: {
            mpz_set(rop, ys[c->args[ref][0]]);
            return;
        }
    }
    mpz_t xres, yres;
    mpz_inits(xres, yres, NULL);
    eval_circ_mod(xres, c, c->args[ref][0], xs, ys, modulus);
    eval_circ_mod(yres, c, c->args[ref][1], xs, ys, modulus);
    switch (op) {
        case ADD: mpz_add(rop, xres, yres); break;
        case SUB: mpz_sub(rop, xres, yres); break;
        case MUL: mpz_mul(rop, xres, yres); break;
    }
    mpz_mod(rop, rop, modulus);
    mpz_clears(xres, yres, NULL);
}

int ensure (circuit *c)
{
    int *res = lin_malloc(c->noutputs * sizeof(int));
    bool ok  = true;

    for (int test_num = 0; test_num < c->ntests; test_num++) {
        bool test_ok = true;

        for (int i = 0; i < c->noutputs; i++) {
            res[i] = eval_circ(c, c->outrefs[i], c->testinps[test_num]);
            // the following uses eval_circ_mod for testing
            /*mpz_t inps[c->ninputs];*/
            /*for (int j = 0; j < c->ninputs; j++) {*/
                /*mpz_init_set_ui(inps[j], c->testinps[test_num][j]);*/
            /*}*/
            /*mpz_t secs[c->nconsts];*/
            /*for (int j = 0; j < c->nconsts; j++) {*/
                /*mpz_init_set_ui(secs[j], c->consts[j]);*/
            /*}*/
            /*mpz_t mod;*/
            /*mpz_init_set_ui(mod, 2);*/
            /*mpz_t tmp;*/
            /*mpz_init(tmp);*/
            /*eval_circ_mod(tmp, c, c->outrefs[i], inps, secs, mod);*/
            /*res[i] = mpz_get_ui(tmp);*/
            test_ok = test_ok && ((res[i] > 0) == c->testouts[test_num][i]);
        }

        if (g_verbose) {
            if (!test_ok)
                printf("\033[1;41m");
            printf("test %d input=", test_num);
            for (int i = 0; i < c->ninputs; i++)
                printf("%d", c->testinps[test_num][i]);
            printf(" expected=");
            for (int i = 0; i < c->noutputs; i++)
                printf("%d", c->testouts[test_num][i]);
            printf(" got=");
            for (int i = 0; i < c->noutputs; i++)
                printf("%d", res[i] > 0);
            if (!test_ok)
                printf("\033[0m");
            puts("");
        }

        ok = ok && test_ok;
    }
    free(res);
    return ok;
}

////////////////////////////////////////////////////////////////////////////////
// circuit topological ordering

void topo_helper (int ref, int *topo, int *seen, int *i, circuit *c)
{
    if (seen[ref]) return;
    operation op = c->ops[ref];
    if (op == ADD || op == SUB || op == MUL) {
        topo_helper(c->args[ref][0], topo, seen, i, c);
        topo_helper(c->args[ref][1], topo, seen, i, c);
    }
    topo[(*i)++] = ref;
    seen[ref]    = 1;
}

void topological_order (int *topo, circuit *c, circref ref)
{
    int *seen = lin_calloc(c->_ref_alloc, sizeof(int));
    int i = 0;
    topo_helper(ref, topo, seen, &i, c);
    free(seen);
}

// dependencies fills an array with the refs to the subcircuit rooted at ref.
// deps is the target array, i is an index into it.
void dependencies_helper (int *deps, int *seen, int *i, circuit *c, int ref)
{
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

int dependencies (int *deps, circuit *c, int ref)
{
    int *seen = lin_calloc(c->nrefs, sizeof(int));
    int ndeps = 0;
    dependencies_helper(deps, seen, &ndeps, c, ref);
    free(seen);
    return ndeps;
}

// returns number of levels
topo_levels* topological_levels (circuit *c, circref root)
{
    topo_levels *topo = lin_malloc(sizeof(topo_levels));
    topo->levels      = lin_malloc(c->nrefs * sizeof(circref));
    topo->level_sizes = lin_calloc(c->nrefs, sizeof(int));

    int *level_alloc  = lin_calloc(c->nrefs, sizeof(int));

    int *topo_list = lin_calloc(c->nrefs, sizeof(int));
    int *deps = lin_malloc(2 * c->nrefs * sizeof(int));

    int max_level = 0;
    topological_order(topo_list, c, root);

    for (int i = 0; i < c->nrefs; i++) {
        int ref = topo_list[i];
        int ndeps = dependencies(deps, c, ref);

        // find the right level for this ref
        for (int j = 0; j < c->nrefs; j++) {
            // if the ref has a dependency in this topological level, try the next level
            bool has_dep = any_in_array(deps, ndeps, topo->levels[j], topo->level_sizes[j]);
            if (has_dep) continue;
            // otherwise the ref belongs on this level

            // ensure space
            if (level_alloc[j] == 0) {
                level_alloc[j] = 2;
                topo->levels[j] = lin_malloc(level_alloc[j] * sizeof(int));
            } else if (topo->level_sizes[j] + 1 >= level_alloc[j]) {
                level_alloc[j] *= 2;
                topo->levels[j] = lin_realloc(topo->levels[j], level_alloc[j] * sizeof(int));
            }

            topo->levels[j][topo->level_sizes[j]] = ref; // push this ref
            topo->level_sizes[j] += 1;

            if (j > max_level)
                max_level = j;

            break; // we've found the right level for this ref: stop searching
        }
    }
    topo->nlevels = max_level + 1;
    free(topo_list);
    free(deps);
    free(level_alloc);
    return topo;
}

void topo_levels_destroy (topo_levels *topo)
{
    for (int i = 0; i < topo->nlevels; i++)
        free(topo->levels[i]);
    free(topo->levels);
    free(topo->level_sizes);
    free(topo);
}

////////////////////////////////////////////////////////////////////////////////
// circuit info calculations

uint32_t depth (circuit *c, circref ref)
{
    operation op = c->ops[ref];
    if (op == XINPUT || op == YINPUT) {
        return 0;
    }
    uint32_t xres = depth(c, c->args[ref][0]);
    uint32_t yres = depth(c, c->args[ref][1]);
    return max(xres, yres);
}

uint32_t degree (circuit *c, circref ref)
{
    operation op = c->ops[ref];
    if (op == XINPUT || op == YINPUT) {
        return 1;
    }
    uint32_t xres = degree(c, c->args[ref][0]);
    uint32_t yres = degree(c, c->args[ref][1]);
    if (op == MUL)
        return xres + yres;
    return max(xres, yres); // else op == ADD || op == SUB
}

uint32_t max_degree (circuit *c)
{
    uint32_t tmp;
    uint32_t ret = 0;
    for (int i = 0; i < c->noutputs; i++) {
        tmp = degree(c, c->outrefs[i]);
        if (tmp > ret)
            ret = tmp;
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
// circuit creation

void circ_add_test (circuit *c, char *inpstr, char *outstr)
{
    if (c->ntests >= c->_test_alloc) {
        c->testinps = lin_realloc(c->testinps, 2 * c->_test_alloc * sizeof(int**));
        c->testouts = lin_realloc(c->testouts, 2 * c->_test_alloc * sizeof(int**));
        c->_test_alloc *= 2;
    }

    int inp_len = strlen(inpstr);
    int out_len = strlen(outstr);
    int *inp = lin_malloc(inp_len * sizeof(int));
    int *out = lin_malloc(out_len * sizeof(int));

    for (int i = 0; i < inp_len; i++)
        if (inpstr[inp_len-1 - i] == '1')
            inp[i] = 1;
        else
            inp[i] = 0;

    for (int i = 0; i < out_len; i++)
        if (outstr[out_len-1 - i] == '1')
            out[i] = 1;
        else
            out[i] = 0;

    c->testinps[c->ntests] = inp;
    c->testouts[c->ntests] = out;
    c->ntests += 1;

    free(inpstr);
    free(outstr);
}

void circ_add_xinput (circuit *c, circref ref, input_id id)
{
    ensure_gate_space(c, ref);
    c->ninputs += 1;
    c->nrefs   += 1;
    c->ops[ref] = XINPUT;
    circref *args = lin_malloc(2 * sizeof(circref));
    args[0] = id;
    args[1] = -1;
    c->args[ref] = args;
}

void circ_add_yinput (circuit *c, circref ref, size_t id, int val)
{
    ensure_gate_space(c, ref);
    if (c->nconsts >= c->_consts_alloc) {
        c->consts = lin_realloc(c->consts, 2 * c->_consts_alloc * sizeof(int));
        c->_consts_alloc *= 2;
    }
    c->consts[c->nconsts] = val;
    c->nconsts  += 1;
    c->nrefs    += 1;
    c->ops[ref]  = YINPUT;
    circref *args = lin_malloc(2 * sizeof(circref));
    args[0] = id;
    args[1] = val;
    c->args[ref] = args;
}

void circ_add_gate (circuit *c, circref ref, operation op, int xref, int yref, bool is_output)
{
    ensure_gate_space(c, ref);
    c->ngates   += 1;
    c->nrefs    += 1;
    c->ops[ref]  = op;
    circref *args = lin_malloc(2 * sizeof(circref));
    args[0] = xref;
    args[1] = yref;
    c->args[ref] = args;
    if (is_output) {
        if (c->noutputs >= c->_outref_alloc) {
            c->outrefs = lin_realloc(c->outrefs, 2 * c->_outref_alloc * sizeof(circref));
            c->_outref_alloc *= 2;
        }
        c->outrefs[c->noutputs++] = ref;
    }
}

////////////////////////////////////////////////////////////////////////////////
// helpers

void ensure_gate_space (circuit *c, circref ref)
{
    if (ref >= c->_ref_alloc) {
        c->args = lin_realloc(c->args, 2 * c->_ref_alloc * sizeof(circref**));
        c->ops  = lin_realloc(c->ops,  2 * c->_ref_alloc * sizeof(operation));
        c->_ref_alloc *= 2;
    }
}

operation str2op (char *s)
{
    if (strcmp(s, "ADD") == 0) {
        return ADD;
    } else if (strcmp(s, "SUB") == 0) {
        return SUB;
    } else if (strcmp(s, "MUL") == 0) {
        return MUL;
    }
    exit(EXIT_FAILURE);
}
