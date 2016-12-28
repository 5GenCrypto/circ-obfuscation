#include "vtables.h"
#include "obf_params.h"

#include <err.h>

struct sp_info {
    const obf_params_t *op;
    level *toplevel;
};

static inline size_t ARRAY_SUM(const size_t *xs, size_t n)
{
    size_t res = 0;
    for (size_t i = 0; i < n; ++i) {
        res += xs[i];
    }
    return res;
}

static int
_sp_init(const mmap_vtable *mmap, secret_params *sp,
         const obf_params_t *op, size_t lambda, aes_randstate_t rng)
{
    size_t t, kappa, nzs;

    sp->info = calloc(1, sizeof(sp_info));
    sp->info->op = op;
    sp->info->toplevel = level_create_vzt(op);
    if (g_verbose) {
        fprintf(stderr, "toplevel: ");
        level_fprint(stderr, sp->info->toplevel);
    }

    t = 0;
    for (size_t o = 0; o < op->gamma; o++) {
        size_t tmp = ARRAY_SUM(op->types[o], op->n + 1);
        if (tmp > t)
            t = tmp;
    }
    kappa = acirc_max_total_degree(op->circ) + op->n + 1;
    nzs = (op->n + op->m + 1) * (op->simple ? 3 : 4);

    fprintf(stderr, "Secret parameter settings:\n");
    fprintf(stderr, "* κ:    %lu\n", kappa);
    fprintf(stderr, "* # Zs: %lu\n", nzs);

    {
        int pows[nzs];
        int res;

        level_flatten(pows, sp->info->toplevel);
        sp->sk = calloc(1, mmap->sk->size);
        res = mmap->sk->init(sp->sk, lambda, kappa, nzs, pows, op->nslots, 1,
                             rng, g_verbose);
        if (res) {
            errx(1, "mmap generation failed");
        }
    }
    return 0;
}

static void
_sp_clear(const mmap_vtable *mmap, secret_params *sp)
{
    level_free(sp->info->toplevel);
    free(sp->info);
    if (sp->sk) {
        mmap->sk->clear(sp->sk);
        free(sp->sk);
    }
}

static const void *
_sp_toplevel(const secret_params *sp)
{
    return sp->info->toplevel;
}

static const void *
_sp_params(const secret_params *sp)
{
    return sp->info->op;
}

static sp_vtable ab_sp_vtable = {
    .mmap = NULL,
    .init = _sp_init,
    .clear = _sp_clear,
    .toplevel = _sp_toplevel,
    .params = _sp_params,
};

const sp_vtable *
ab_get_sp_vtable(const mmap_vtable *mmap)
{
    ab_sp_vtable.mmap = mmap;
    return &ab_sp_vtable;
}
