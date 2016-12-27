#include "vtables.h"
#include "dbg.h"
#include "level.h"
#include "obf_params.h"

extern unsigned int g_verbose;

struct sp_info {
    const obf_params_t *op;
    level *toplevel;
};

static inline size_t ARRAY_SUM(size_t *xs, size_t n)
{
    size_t res = 0;
    for (size_t i = 0; i < n; ++i) {
        res += xs[i];
    }
    return res;
}

static int
ab_sp_init(const mmap_vtable *const mmap,
           secret_params *const sp, const obf_params_t *const op,
           size_t lambda, aes_randstate_t rng)
{
    size_t t, kappa, nzs;

    sp->info = calloc(1, sizeof(sp_info));
    sp->info->op = op;
    sp->info->toplevel = level_create_vzt(op);
    printf("toplevel:\n");
    level_print(sp->info->toplevel);

    t = 0;
    for (size_t o = 0; o < op->gamma; o++) {
        size_t tmp = ARRAY_SUM(op->types[o], op->n + 1);
        if (tmp > t)
            t = tmp;
    }
    kappa = acirc_max_total_degree(op->circ) + op->n + 1;
    log_info("kappa=%lu", kappa);
    nzs = (op->n + op->m + 1) * (op->simple ? 3 : 4);
    log_info("nzs=%lu", nzs);

    {
        int pows[nzs];
        int res;

        level_flatten(pows, sp->info->toplevel);
        sp->sk = calloc(1, mmap->sk->size);
        res = mmap->sk->init(sp->sk, lambda, kappa, nzs, pows, op->nslots, 1,
                             rng, g_verbose);
        if (res) {
            log_err("mmap generation failed\n");
            free(sp->sk);
            sp->sk = NULL;
            return 1;
        }
    }
    return 0;
}

static void
ab_sp_clear(const mmap_vtable *const mmap, secret_params *sp)
{
    level_free(sp->info->toplevel);
    free(sp->info);
    if (sp->sk) {
        mmap->sk->clear(sp->sk);
        free(sp->sk);
    }
}

static void *
ab_sp_toplevel(const secret_params *const sp)
{
    return sp->info->toplevel;
}

static void *
ab_sp_params(const secret_params *const sp)
{
    return sp->info->op;
}

static sp_vtable ab_sp_vtable = {
    .mmap = NULL,
    .init = ab_sp_init,
    .clear = ab_sp_clear,
    .toplevel = ab_sp_toplevel,
    .params = ab_sp_params,
};

const sp_vtable *
ab_get_sp_vtable(const mmap_vtable *const mmap)
{
    ab_sp_vtable.mmap = mmap;
    return &ab_sp_vtable;
}
