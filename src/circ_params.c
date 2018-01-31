#include "circ_params.h"
#include "util.h"

#include <assert.h>

int
circ_params_init(circ_params_t *cp, size_t n, acirc_t *circ)
{
    cp->nslots = n;
    cp->circ = circ;
    if ((cp->ds = my_calloc(n, sizeof cp->ds[0])) == NULL) goto error;
    if ((cp->qs = my_calloc(n, sizeof cp->ds[0])) == NULL) goto error;
    return OK;
error:
    circ_params_clear(cp);
    return ERR;
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
circ_params_fwrite(const circ_params_t *cp, FILE *fp)
{
    if (size_t_fwrite(cp->nslots, fp) == ERR) goto error;
    for (size_t i = 0; i < acirc_nsymbols(cp->circ); ++i)
        if (size_t_fwrite(cp->ds[i], fp) == ERR) goto error;
    for (size_t i = 0; i < acirc_nsymbols(cp->circ); ++i)
        if (size_t_fwrite(cp->qs[i], fp) == ERR) goto error;
    return OK;
error:
    fprintf(stderr, "%s: %s: writing circuit parameters failed\n",
            errorstr, __func__);
    return ERR;
}

int
circ_params_fread(circ_params_t *cp, acirc_t *circ, FILE *fp)
{
    if (size_t_fread(&cp->nslots, fp) == ERR) goto error;
    cp->ds = my_calloc(acirc_nsymbols(circ), sizeof cp->ds[0]);
    cp->qs = my_calloc(acirc_nsymbols(circ), sizeof cp->qs[0]);
    for (size_t i = 0; i < acirc_nsymbols(circ); ++i)
        if (size_t_fread(&cp->ds[i], fp) == ERR) goto error;
    for (size_t i = 0; i < acirc_nsymbols(circ); ++i)
        if (size_t_fread(&cp->qs[i], fp) == ERR) goto error;
    cp->circ = circ;
    return OK;
error:
    circ_params_clear(cp);
    fprintf(stderr, "%s: %s: reading circuit parameters failed\n",
            errorstr, __func__);
    return ERR;
}

size_t
circ_params_ninputs(const circ_params_t *cp)
{
    size_t ninputs = 0;
    for (size_t i = 0; i < cp->nslots; ++i) {
        ninputs += cp->ds[i];
    }
    return ninputs;
}

size_t
circ_params_slot(const circ_params_t *cp, size_t pos)
{
    size_t total = 0;
    for (size_t i = 0; i < cp->nslots; ++i) {
        if (pos >= total && pos < total + cp->ds[i])
            return i;
        total += cp->ds[i];
    }
    fprintf(stderr, "%s: %s: fatal: slot not found?\n",
            errorstr, __func__);
    abort();
}

size_t
circ_params_bit(const circ_params_t *cp, size_t pos)
{
    size_t total = 0;
    for (size_t i = 0; i < cp->nslots; ++i) {
        if (pos >= total && pos < total + cp->ds[i])
            return pos - total;
        total += cp->ds[i];
    }
    fprintf(stderr, "%s: %s: fatal: bit not found?\n",
            errorstr, __func__);
    abort();
}

int
circ_params_print(const circ_params_t *cp)
{
    const size_t has_consts = acirc_nconsts(cp->circ) + acirc_nsecrets(cp->circ) ? 1 : 0;
    fprintf(stderr, "Circuit parameters:\n");
    fprintf(stderr, "* # inputs:...... %lu\n", acirc_ninputs(cp->circ));
    fprintf(stderr, "* # consts:...... %lu\n", acirc_nconsts(cp->circ));
    fprintf(stderr, "* # secrets:..... %lu\n", acirc_nsecrets(cp->circ));
    fprintf(stderr, "* # outputs: .... %lu\n", acirc_noutputs(cp->circ));
    fprintf(stderr, "* # symbols: .... %lu  [", acirc_nsymbols(cp->circ));
    for (size_t i = 0; i < acirc_nsymbols(cp->circ); ++i) {
        fprintf(stderr, " %lu ", acirc_symlen(cp->circ, i));
    }
    fprintf(stderr, "]\n");
    fprintf(stderr, "* # slots: ...... %lu\n", cp->nslots);
    for (size_t i = 0; i < cp->nslots; ++i) {
        size_t degree;
        if (i == cp->nslots - has_consts)
            degree = acirc_max_const_degree(cp->circ);
        else
            degree = acirc_max_var_degree(cp->circ, i);
        if (degree == 0) return ERR; /* happens if we fail to parse the circuit */
        fprintf(stderr, "*   slot %lu: ..... %lu (%lu) [%lu]\n", i,
                cp->ds[i], cp->qs[i], degree);
    }
    fprintf(stderr, "* # refs: ....... %lu\n", acirc_nrefs(cp->circ));
    fprintf(stderr, "* # gates: ...... %lu\n", acirc_ngates(cp->circ));
    fprintf(stderr, "* # muls: ....... %lu\n", acirc_nmuls(cp->circ));
    fprintf(stderr, "* depth: ........ %lu\n", acirc_max_depth(cp->circ));
    fprintf(stderr, "* saved? ........ %s", acirc_is_saved(cp->circ) ? "✓" : "✗");
    if (acirc_is_saved(cp->circ))
        fprintf(stderr, " [%s.encodings/]", acirc_fname(cp->circ));
    fprintf(stderr, "\n");
    fprintf(stderr, "* binary? ....... %s\n", acirc_is_binary(cp->circ) ? "✓" : "✗");
    return OK;
}
