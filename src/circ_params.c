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
circ_params_ninputs(const acirc_t *circ)
{
    size_t ninputs = 0;
    for (size_t i = 0; i < acirc_nslots(circ); ++i)
        ninputs += acirc_symlen(circ, i);
    return ninputs;
}

size_t
circ_params_slot(const acirc_t *circ, size_t pos)
{
    size_t total = 0;
    for (size_t i = 0; i < acirc_nslots(circ); ++i) {
        if (pos >= total && pos < total + acirc_symlen(circ, i))
            return i;
        total += acirc_symlen(circ, i);
    }
    fprintf(stderr, "%s: %s: fatal: slot not found?\n",
            errorstr, __func__);
    abort();
}

size_t
circ_params_bit(const acirc_t *circ, size_t pos)
{
    size_t total = 0;
    for (size_t i = 0; i < acirc_nslots(circ); ++i) {
        if (pos >= total && pos < total + acirc_symlen(circ, i))
            return pos - total;
        total += acirc_symlen(circ, i);
    }
    fprintf(stderr, "%s: %s: fatal: bit not found?\n",
            errorstr, __func__);
    abort();
}

int
circ_params_print(const acirc_t *circ)
{
    const size_t has_consts = acirc_nconsts(circ) + acirc_nsecrets(circ) ? 1 : 0;
    fprintf(stderr, "Circuit parameters:\n");
    fprintf(stderr, "* # inputs:...... %lu\n", acirc_ninputs(circ));
    fprintf(stderr, "* # consts:...... %lu\n", acirc_nconsts(circ));
    fprintf(stderr, "* # secrets:..... %lu\n", acirc_nsecrets(circ));
    fprintf(stderr, "* # outputs: .... %lu\n", acirc_noutputs(circ));
    fprintf(stderr, "* # symbols: .... %lu  [", acirc_nsymbols(circ));
    for (size_t i = 0; i < acirc_nsymbols(circ); ++i) {
        fprintf(stderr, " %lu ", acirc_symlen(circ, i));
    }
    fprintf(stderr, "]\n");
    fprintf(stderr, "* # slots: ...... %lu\n", acirc_nslots(circ));
    for (size_t i = 0; i < acirc_nslots(circ); ++i) {
        size_t degree;
        if (i == acirc_nslots(circ) - has_consts)
            degree = acirc_max_const_degree(circ);
        else
            degree = acirc_max_var_degree(circ, i);
        if (degree == 0) return ERR; /* happens if we fail to parse the circuit */
        fprintf(stderr, "*   slot %lu: ..... %lu (%lu) [%lu]\n", i,
                acirc_symlen(circ, i), acirc_symnum(circ, i), degree);
    }
    fprintf(stderr, "* # refs: ....... %lu\n", acirc_nrefs(circ));
    fprintf(stderr, "* # gates: ...... %lu\n", acirc_ngates(circ));
    fprintf(stderr, "* # muls: ....... %lu\n", acirc_nmuls(circ));
    fprintf(stderr, "* depth: ........ %lu\n", acirc_max_depth(circ));
    fprintf(stderr, "* saved? ........ %s", acirc_is_saved(circ) ? "✓" : "✗");
    if (acirc_is_saved(circ))
        fprintf(stderr, " [%s.encodings/]", acirc_fname(circ));
    fprintf(stderr, "\n");
    fprintf(stderr, "* binary? ....... %s\n", acirc_is_binary(circ) ? "✓" : "✗");
    return OK;
}
