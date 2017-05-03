#include "circ_params.h"
#include "util.h"

int
circ_params_init(circ_params_t *cp, size_t n, const acirc *circ)
{
    cp->n = n;
    cp->m = circ->outputs.n;
    cp->circ = circ;
    cp->ds = my_calloc(n, sizeof cp->ds[0]);
    cp->qs = my_calloc(n, sizeof cp->ds[0]);
    if (cp->ds == NULL)
        return ERR;
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
    fprintf(stderr, "* ninputs:...... %lu\n", circ_params_ninputs(cp));
    fprintf(stderr, "* nslots: ...... %lu\n", cp->n);
    for (size_t i = 0; i < cp->n; ++i) {
        fprintf(stderr, "*   slot #%lu: ..... %lu (%lu)\n", i + 1,
                cp->ds[i], cp->qs[i]);
    }
    fprintf(stderr, "* nconsts:...... %lu\n", cp->circ->consts.n);

    fprintf(stderr, "* noutputs: .... %lu\n", cp->m);
    fprintf(stderr, "* ngates: ...... %lu\n", cp->circ->gates.n);
    fprintf(stderr, "* nmuls: ....... %lu\n", acirc_nmuls(cp->circ));
    fprintf(stderr, "* depth: ....... %lu\n", acirc_max_depth(cp->circ));
    fprintf(stderr, "* degree: ...... %lu\n", acirc_max_degree(cp->circ));

}
