#include "circ_params.h"
#include "util.h"

#include <assert.h>

int
circ_params_init(circ_params_t *cp, size_t n, acirc *circ)
{
    cp->n = n;
    cp->c = circ->consts.n;
    cp->m = circ->outputs.n;
    cp->circ = circ;
    cp->ds = my_calloc(n, sizeof cp->ds[0]);
    cp->qs = my_calloc(n, sizeof cp->ds[0]);
    return OK;
}

void
circ_params_clear(circ_params_t *cp)
{
    if (cp->ds)
        free(cp->ds);
    if (cp->qs)
        free(cp->qs);
}

int
circ_params_fwrite(const circ_params_t *const cp, FILE *fp)
{
    fprintf(fp, "%lu\n", cp->n);
    fprintf(fp, "%lu\n", cp->c);
    fprintf(fp, "%lu\n", cp->m);
    for (size_t i = 0; i < cp->n; ++i) {
        fprintf(fp, "%lu\n", cp->ds[i]);
    }
    for (size_t i = 0; i < cp->n; ++i) {
        fprintf(fp, "%lu\n", cp->qs[i]);
    }
    return OK;
}

int
circ_params_fread(circ_params_t *const cp, acirc *circ, FILE *fp)
{
    fscanf(fp, "%lu\n", &cp->n);
    fscanf(fp, "%lu\n", &cp->c);
    fscanf(fp, "%lu\n", &cp->m);
    cp->ds = my_calloc(cp->n, sizeof cp->ds[0]);
    cp->qs = my_calloc(cp->n, sizeof cp->qs[0]);
    for (size_t i = 0; i < cp->n; ++i) {
        fscanf(fp, "%lu\n", &cp->ds[i]);
    }
    for (size_t i = 0; i < cp->n; ++i) {
        fscanf(fp, "%lu\n", &cp->qs[i]);
    }
    cp->circ = circ;
    return OK;
}

size_t
circ_params_ninputs(const circ_params_t *cp)
{
    size_t ninputs = 0;
    for (size_t i = 0; i < cp->n; ++i) {
        ninputs += cp->ds[i];
    }
    return ninputs;
}

size_t
circ_params_slot(const circ_params_t *cp, size_t pos)
{
    size_t total = 0;
    for (size_t i = 0; i < cp->n; ++i) {
        if (pos >= total && pos < total + cp->ds[i])
            return i;
        total += cp->ds[i];
    }
    abort();
}

size_t
circ_params_bit(const circ_params_t *cp, size_t pos)
{
    size_t total = 0;
    for (size_t i = 0; i < cp->n; ++i) {
        if (pos >= total && pos < total + cp->ds[i])
            return pos - total;
        total += cp->ds[i];
    }
    abort();
}

void
circ_params_print(const circ_params_t *cp)
{
    fprintf(stderr, "Circuit parameters:\n");
    fprintf(stderr, "* ninputs:...... %lu\n", cp->circ->ninputs);
    fprintf(stderr, "* nslots: ...... %lu\n", cp->n);
    for (size_t i = 0; i < cp->n; ++i) {
        fprintf(stderr, "*   slot #%lu: ..... %lu (%lu)\n", i + 1,
                cp->ds[i], cp->qs[i]);
    }
    fprintf(stderr, "* nconsts:...... %lu\n", cp->c);

    fprintf(stderr, "* noutputs: .... %lu\n", cp->m);
    fprintf(stderr, "* ngates: ...... %lu\n", cp->circ->gates.n);
    fprintf(stderr, "* nmuls: ....... %lu\n", acirc_nmuls(cp->circ));
    fprintf(stderr, "* depth: ....... %lu\n", acirc_max_depth(cp->circ));
    fprintf(stderr, "* degree: ...... %lu\n", acirc_max_degree(cp->circ));

}
