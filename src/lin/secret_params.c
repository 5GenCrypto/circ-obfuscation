#include "vtables.h"
#include "obf_params.h"
#include "../util.h"

struct sp_info {
    const obf_params_t *op;
    level *toplevel;
};
#define info(x) (x)->info

static int
_sp_init(const mmap_vtable *mmap, secret_params *const sp,
         const obf_params_t *const op, size_t lambda, aes_randstate_t rng)
{
    size_t t, kappa, nzs;

    info(sp) = calloc(1, sizeof(sp_info));
    info(sp)->op = op;
    info(sp)->toplevel = level_create_vzt(op);
    if (g_verbose) {
        fprintf(stderr, "toplevel: ");
        level_fprint(stderr, info(sp)->toplevel);
    }

    t = 0;
    for (size_t o = 0; o < op->gamma; o++) {
        size_t tmp = array_sum(op->types[o], op->c+1);
        if (tmp > t)
            t = tmp;
    }
    kappa = t + op->D - 1;
    nzs = (op->q+1) * (op->c+2) + op->gamma;

    fprintf(stderr, "Secret parameter settings:\n");
    fprintf(stderr, "* t:    %lu\n", t);
    fprintf(stderr, "* κ:    %lu\n", kappa);
    fprintf(stderr, "* # Zs: %lu\n", nzs);

    {
        int pows[nzs];
        int res;

        level_flatten(pows, info(sp)->toplevel);
        sp->sk = my_calloc(1, mmap->sk->size);
        res = mmap->sk->init(sp->sk, lambda, kappa, nzs, pows, op->c + 3, 1,
                             rng, g_verbose);
        if (res) {
            fprintf(stderr, "[%s] mmap generation failed\n", __func__);
            free(sp->sk);
            sp->sk = NULL;
            return 1;
        }
    }
    return 0;
}

static void
_sp_clear(const mmap_vtable *mmap, secret_params *sp)
{
    if (sp == NULL)
        return;
    if (info(sp)->toplevel)
        level_free(info(sp)->toplevel);
    if (info(sp))
        free(info(sp));
    if (sp->sk) {
        mmap->sk->clear(sp->sk);
        free(sp->sk);
    }
}

static const void *
_sp_toplevel(const secret_params *sp)
{
    return info(sp)->toplevel;
}

static const void *
_sp_params(const secret_params *sp)
{
    return info(sp)->op;
}

static sp_vtable lin_sp_vtable = {
    .mmap = NULL,
    .init = _sp_init,
    .clear = _sp_clear,
    .toplevel = _sp_toplevel,
    .params = _sp_params,
};

const sp_vtable *
lin_get_sp_vtable(const mmap_vtable *mmap)
{
    lin_sp_vtable.mmap = mmap;
    return &lin_sp_vtable;
}

