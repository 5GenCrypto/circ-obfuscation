#include "circ_params.h"
#include "util.h"

int
circ_params_init(circ_params_t *cp, size_t n, size_t m, const acirc *circ)
{
    cp->n = n;
    cp->m = m;
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
