#include "circ_params.h"
#include "util.h"

#include <assert.h>

int
circ_params_init(circ_params_t *cp, size_t n, acirc_t *circ)
{
    cp->n = n;
    cp->c = acirc_nconsts(circ);
    cp->m = acirc_noutputs(circ);
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
    if (size_t_fwrite(cp->n, fp) == ERR)
        goto error;
    if (size_t_fwrite(cp->c, fp) == ERR)
        goto error;
    if (size_t_fwrite(cp->m, fp) == ERR)
        goto error;
    for (size_t i = 0; i < cp->n; ++i) {
        if (size_t_fwrite(cp->ds[i], fp) == ERR)
            goto error;
    }
    for (size_t i = 0; i < cp->n; ++i) {
        if (size_t_fwrite(cp->qs[i], fp) == ERR)
            goto error;
    }
    return OK;
error:
    fprintf(stderr, "error: writing circuit parameters failed\n");
    return ERR;
}

int
circ_params_fread(circ_params_t *const cp, acirc_t *circ, FILE *fp)
{
    if (size_t_fread(&cp->n, fp) == ERR)
        goto error;
    if (size_t_fread(&cp->c, fp) == ERR)
        goto error;
    if (size_t_fread(&cp->m, fp) == ERR)
        goto error;
    cp->ds = my_calloc(cp->n, sizeof cp->ds[0]);
    cp->qs = my_calloc(cp->n, sizeof cp->qs[0]);
    for (size_t i = 0; i < cp->n; ++i) {
        if (size_t_fread(&cp->ds[i], fp) == ERR)
            goto error;
    }
    for (size_t i = 0; i < cp->n; ++i) {
        if (size_t_fread(&cp->qs[i], fp) == ERR)
            goto error;
    }
    cp->circ = circ;
    return OK;
error:
    if (cp->ds)
        free(cp->ds);
    if (cp->qs)
        free(cp->qs);
    fprintf(stderr, "error: reading circuit parameters failed\n");
    return ERR;
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
    fprintf(stderr, "* ninputs:...... %lu\n", acirc_ninputs(cp->circ));
    fprintf(stderr, "* nslots: ...... %lu\n", cp->n);
    for (size_t i = 0; i < cp->n; ++i) {
        fprintf(stderr, "*   slot #%lu: ..... %lu (%lu)\n", i + 1,
                cp->ds[i], cp->qs[i]);
    }
    fprintf(stderr, "* nconsts:...... %lu\n", cp->c);
    fprintf(stderr, "* noutputs: .... %lu\n", cp->m);
    fprintf(stderr, "* ngates: ...... %lu\n", acirc_ngates(cp->circ));
    fprintf(stderr, "* nmuls: ....... %lu\n", acirc_nmuls(cp->circ));
    fprintf(stderr, "* depth: ....... %lu\n", acirc_max_depth(cp->circ));
    fprintf(stderr, "* degree: ...... %lu\n", acirc_max_degree(cp->circ));
}
