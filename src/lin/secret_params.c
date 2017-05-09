#include "obf_params.h"
#include "level.h"
#include "vtables.h"
#include "mmap.h"
#include "util.h"

struct sp_info {
    level *toplevel;
    const circ_params_t *cp;
};
#define spinfo(x) (x)->info

static int
_sp_init(secret_params *sp, mmap_params_t *mp, const obf_params_t *op,
         size_t kappa)
{
    const circ_params_t *cp = &op->cp;
    const size_t ninputs = cp->n - (cp->c ? 1 : 0);
    const size_t noutputs = cp->m;
    const size_t q = array_max(cp->qs, ninputs);
    size_t t;

    spinfo(sp) = my_calloc(1, sizeof spinfo(sp)[0]);
    spinfo(sp)->toplevel = level_create_vzt(cp, op->M, op->D);
    spinfo(sp)->cp = cp;

    t = 0;
    for (size_t o = 0; o < noutputs; o++) {
        size_t tmp = array_sum(op->types[o], ninputs + 1);
        if (tmp > t)
            t = tmp;
    }
    /* EC:Lin16, pg. 45 */
    mp->kappa = kappa ? kappa : (2 + ninputs + t + op->D);
    mp->nzs = (q + 1) * (ninputs + 2) + noutputs;
    mp->pows = my_calloc(mp->nzs, sizeof mp->pows[0]);
    if (level_flatten(mp->pows, spinfo(sp)->toplevel) == ERR) {
        fprintf(stderr, "error: toplevel overflow\n");
        goto error;
    }
    mp->my_pows = true;
    mp->nslots = ninputs + 3;
    return OK;
error:
    free(mp->pows);
    level_free(spinfo(sp)->toplevel);
    free(spinfo(sp));
    return ERR;
}

static void
_sp_clear(secret_params *sp)
{
    if (spinfo(sp)->toplevel)
        level_free(spinfo(sp)->toplevel);
    if (spinfo(sp))
        free(spinfo(sp));
}

static const void *
_sp_toplevel(const secret_params *sp)
{
    return spinfo(sp)->toplevel;
}

static const void *
_sp_params(const secret_params *sp)
{
    return spinfo(sp)->cp;
}

static sp_vtable _sp_vtable = {
    .mmap = NULL,
    .init = _sp_init,
    .clear = _sp_clear,
    .toplevel = _sp_toplevel,
    .params = _sp_params,
};

PRIVATE const sp_vtable *
get_sp_vtable(const mmap_vtable *mmap)
{
    _sp_vtable.mmap = mmap;
    return &_sp_vtable;
}

