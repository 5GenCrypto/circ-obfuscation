#include "secret_params.h"

#include "obf_index.h"
#include "obf_params.h"
#include "vtables.h"

struct sp_info {
    obf_index *toplevel;
    const obf_params_t *op;
};
#define my(x) (x)->info

static int
_sp_init(const mmap_vtable *const mmap,
         secret_params *const sp, const obf_params_t *const op,
         size_t lambda, aes_randstate_t rng)
{
    size_t kappa;

    sp->info = calloc(1, sizeof(sp_info));
    sp->info->toplevel = obf_index_create_toplevel(op->circ);
    sp->info->op = op;

    kappa = acirc_delta(op->circ) + op->circ->ninputs;

    if (g_verbose) {
        fprintf(stderr, "Secret parameter settings:\n");
        fprintf(stderr, "* Δ = %lu\n", acirc_delta(op->circ));
        fprintf(stderr, "* n = %lu\n", op->circ->ninputs);
        fprintf(stderr, "* κ = %lu\n", kappa);
    }

    sp->sk = calloc(1, mmap->sk->size);
    (void) mmap->sk->init(sp->sk, lambda, kappa, my(sp)->toplevel->nzs,
                          (int *) my(sp)->toplevel->pows, 2, 1, rng, g_verbose);
    return 0;
}

static void
_sp_clear(const mmap_vtable *const mmap, secret_params *sp)
{
    (void) mmap;
    obf_index_destroy(my(sp)->toplevel);
    free(sp->info);
    mmap->sk->clear(sp->sk);
    free(sp->sk);
}

static const void *
_sp_toplevel(const secret_params *const sp)
{
    return my(sp)->toplevel;
}

static const void *
_sp_params(const secret_params *const sp)
{
    (void) sp;
    return NULL;
}

static sp_vtable zim_sp_vtable = {
    .mmap = NULL,
    .init = _sp_init,
    .clear = _sp_clear,
    .toplevel = _sp_toplevel,
    .params = _sp_params,
};

sp_vtable *
zim_get_sp_vtable(const mmap_vtable *const mmap)
{
    zim_sp_vtable.mmap = mmap;
    return &zim_sp_vtable;
}

const obf_params_t *
zim_sp_op(const secret_params *const sp)
{
    return my(sp)->op;
}
