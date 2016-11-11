#include "obfuscate.h"

#include "dbg.h"
#include "util.h"

#include <assert.h>
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct obfuscation {
    const mmap_vtable *mmap;
    const obf_params_t *op;
    secret_params *sp;
    public_params *pp;
    aes_randstate_t rng;
    encoding ***R_ib;
    encoding ***Z_ib;
    encoding ****R_hat_ib_o;
    encoding ****Z_hat_ib_o;
    encoding **R_i;
    encoding **Z_i;
    encoding **R_o_i;
    encoding **Z_o_i;
    encoding **R_hat_o_i;
    encoding **Z_hat_o_i;
};

static void
encode_v_ib(const mmap_vtable *mmap, encoding *enc, secret_params *p, mpz_t *rs,
            size_t i, bool b)
{
    level *lvl;

    lvl = level_create_v_ib(p->op, i, b);
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_ib_v_star(const mmap_vtable *mmap, encoding *enc, secret_params *p,
                   mpz_t *rs, size_t i, bool b)
{
    level *lvl;

    lvl = level_create_v_ib(p->op, i, b);
    if (!p->op->simple) {
        level *v_star;

        v_star = level_create_v_star(p->op);
        level_add(lvl, lvl, v_star);
        level_free(v_star);
    }
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_hat_ib_o(const mmap_vtable *mmap, encoding *enc, secret_params *p,
                  mpz_t *rs, size_t i, bool b, size_t o)
{
    level *lvl;

    lvl = level_create_v_hat_ib_o(p->op, i, b, o);
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_hat_ib_o_v_star(const mmap_vtable *mmap, encoding *enc, secret_params *p,
                         mpz_t *rs, size_t i, bool b, size_t o)
{
    level *lvl;

    lvl = level_create_v_hat_ib_o(p->op, i, b, o);
    if (!p->op->simple) {
        level *v_star;

        v_star = level_create_v_star(p->op);
        level_add(lvl, lvl, v_star);
        level_free(v_star);
    }
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_i(const mmap_vtable *mmap, encoding *enc, secret_params *p, mpz_t *rs,
            size_t i)
{
    level *lvl;

    lvl = level_create_v_i(p->op, p->op->n + i);
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_i_v_star(const mmap_vtable *mmap, encoding *enc, secret_params *p,
                  mpz_t *rs, size_t i)
{
    level *lvl;

    lvl = level_create_v_i(p->op, p->op->n + i);
    if (!p->op->simple) {
        level *v_star;

        v_star = level_create_v_star(p->op);
        level_add(lvl, lvl, v_star);
        level_free(v_star);
    }
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_hat_o(const mmap_vtable *mmap, encoding *enc, secret_params *p,
               mpz_t *rs, size_t o)
{
    level *lvl;
    lvl = level_create_v_hat_o(p->op, o);
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_hat_o_v_star(const mmap_vtable *mmap, encoding *enc, secret_params *p,
                      mpz_t *rs, size_t o)
{
    level *lvl;

    lvl = level_create_v_hat_o(p->op, o);
    if (!p->op->simple) {
        level *v_star;

        v_star = level_create_v_star(p->op);
        level_mul_ui(v_star, v_star, p->op->D);
        level_add(lvl, lvl, v_star);
        level_free(v_star);
    }
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

obfuscation *
obfuscation_new(const mmap_vtable *mmap, const obf_params_t *const op,
                size_t secparam)
{
    obfuscation *obf;

    obf = malloc(sizeof(obfuscation));
    obf->mmap = mmap;
    obf->op = op;
    obf->sp = malloc(sizeof(secret_params));
    obf->pp = malloc(sizeof(public_params));
    aes_randinit(obf->rng);
    secret_params_init(mmap, obf->sp, op, secparam, obf->rng);
    public_params_init(mmap, obf->pp, obf->sp);

    obf->R_ib = lin_malloc(op->n * sizeof(encoding **));
    for (size_t i = 0; i < op->n; i++) {
        obf->R_ib[i] = lin_malloc(2 * sizeof(encoding *));
        for (size_t b = 0; b < 2; b++) {
            obf->R_ib[i][b] = encoding_new(mmap, obf->pp);
        }
    }
    obf->Z_ib = lin_malloc(op->n * sizeof(encoding **));
    for (size_t i = 0; i < op->n; i++) {
        obf->Z_ib[i] = lin_malloc(2 * sizeof(encoding *));
        for (size_t b = 0; b < 2; b++) {
            obf->Z_ib[i][b] = encoding_new(mmap, obf->pp);
        }
    }
    obf->R_hat_ib_o = lin_malloc(op->gamma * sizeof(encoding ***));
    for (size_t o = 0; o < op->gamma; ++o) {
        obf->R_hat_ib_o[o] = lin_malloc(op->n * sizeof(encoding **));
        for (size_t i = 0; i < op->n; i++) {
            obf->R_hat_ib_o[o][i] = lin_malloc(2 * sizeof(encoding *));
            for (size_t b = 0; b < 2; b++) {
                obf->R_hat_ib_o[o][i][b] = encoding_new(mmap, obf->pp);
            }
        }
    }
    obf->Z_hat_ib_o = lin_malloc(op->gamma * sizeof(encoding ***));
    for (size_t o = 0; o < op->gamma; ++o) {
        obf->Z_hat_ib_o[o] = lin_malloc(op->n * sizeof(encoding **));
        for (size_t i = 0; i < op->n; i++) {
            obf->Z_hat_ib_o[o][i] = lin_malloc(2 * sizeof(encoding *));
            for (size_t b = 0; b < 2; b++) {
                obf->Z_hat_ib_o[o][i][b] = encoding_new(mmap, obf->pp);
            }
        }
    }
    obf->R_i = lin_malloc(op->m * sizeof(encoding *));
    for (size_t i = 0; i < op->m; i++) {
        obf->R_i[i] = encoding_new(mmap, obf->pp);
    }
    obf->Z_i = lin_malloc(op->m * sizeof(encoding *));
    for (size_t i = 0; i < op->m; i++) {
        obf->Z_i[i] = encoding_new(mmap, obf->pp);
    }
    obf->R_o_i = lin_malloc(op->gamma * sizeof(encoding *));
    for (size_t i = 0; i < op->gamma; i++) {
        obf->R_o_i[i] = encoding_new(mmap, obf->pp);
    }
    obf->Z_o_i = lin_malloc(op->gamma * sizeof(encoding *));
    for (size_t i = 0; i < op->gamma; i++) {
        obf->Z_o_i[i] = encoding_new(mmap, obf->pp);
    }

    return obf;
}

void
obfuscation_free(obfuscation *obf)
{
    if (obf == NULL)
        return;

    const obf_params_t *op = obf->op;

    if (obf->R_ib) {
        for (size_t i = 0; i < op->n; i++) {
            for (size_t b = 0; b < 2; b++) {
                encoding_free(obf->mmap, obf->R_ib[i][b]);
            }
            free(obf->R_ib[i]);
        }
        free(obf->R_ib);
    }

    if (obf->Z_ib) {
        for (size_t i = 0; i < op->n; i++) {
            for (size_t b = 0; b < 2; b++) {
                encoding_free(obf->mmap, obf->Z_ib[i][b]);
            }
            free(obf->Z_ib[i]);
        }
        free(obf->Z_ib);
    }
    if (obf->R_hat_ib_o) {
        for (size_t o = 0; o < op->gamma; ++o) {
            for (size_t i = 0; i < op->n; i++) {
                for (size_t b = 0; b < 2; b++) {
                    encoding_free(obf->mmap, obf->R_hat_ib_o[o][i][b]);
                }
                free(obf->R_hat_ib_o[o][i]);
            }
            free(obf->R_hat_ib_o[o]);
        }
        free(obf->R_hat_ib_o);
    }
    if (obf->Z_hat_ib_o) {
        for (size_t o = 0; o < op->gamma; ++o) {
            for (size_t i = 0; i < op->n; i++) {
                for (size_t b = 0; b < 2; b++) {
                    encoding_free(obf->mmap, obf->Z_hat_ib_o[o][i][b]);
                }
                free(obf->Z_hat_ib_o[o][i]);
            }
            free(obf->Z_hat_ib_o[o]);
        }
        free(obf->Z_hat_ib_o);
    }
    if (obf->R_i) {
        for (size_t i = 0; i < op->m; i++) {
            encoding_free(obf->mmap, obf->R_i[i]);
        }
        free(obf->R_i);
    }
    if (obf->Z_i) {
        for (size_t i = 0; i < op->m; i++) {
            encoding_free(obf->mmap, obf->Z_i[i]);
        }
        free(obf->Z_i);
    }
    if (obf->R_o_i) {
        for (size_t i = 0; i < op->gamma; i++) {
            encoding_free(obf->mmap, obf->R_o_i[i]);
        }
        free(obf->R_o_i);
    }
    if (obf->Z_o_i) {
        for (size_t i = 0; i < op->gamma; i++) {
            encoding_free(obf->mmap, obf->Z_o_i[i]);
        }
        free(obf->Z_o_i);
    }
    if (obf->pp) {
        public_params_clear(obf->mmap, obf->pp);
        free(obf->pp);
    }
    if (obf->sp) {
        secret_params_clear(obf->mmap, obf->sp);
        free(obf->sp);
    }
    /* if (obf->rng) */
    /*     aes_randclear(obf->rng); */
    free(obf);
}

void
obfuscate(obfuscation *obf)
{
    mpz_t *moduli = mpz_vect_create_of_fmpz(obf->mmap->sk->plaintext_fields(obf->sp->sk),
                                            obf->mmap->sk->nslots(obf->sp->sk));
    size_t n = obf->op->n,
           m = obf->op->m,
           gamma = obf->op->gamma,
           nslots = obf->op->simple ? 2 : (n + 2);
    mpz_t ys[n + m], w_hats[n][nslots];

    /* gmp_printf("moduli[0]: %Zd\n", moduli[0]); */
    /* gmp_printf("moduli[1]: %Zd\n", moduli[1]); */

    mpz_vect_init(ys, n + m);
    for (size_t i = 0; i < n + m; ++i) {
        mpz_urandomm_aes(ys[i], obf->rng, moduli[0]);
    }

    for (size_t i = 0; i < n; ++i) {
        mpz_vect_init(w_hats[i], nslots);
        mpz_urandomm_vect_aes(w_hats[i], moduli, nslots, obf->rng);
        if (!obf->op->simple) {
            mpz_set_ui(w_hats[i][i + 2], 0);
        }
    }

    /* encode R_ib and Z_ib */
    for (size_t i = 0; i < n; i++) {
        for (size_t b = 0; b < 2; b++) {
            mpz_t tmp[nslots], other[nslots];
            mpz_vect_init(tmp, nslots);
            mpz_vect_init(other, nslots);
            /* Compute R_i,b encodings */
            mpz_urandomm_vect_aes(tmp, moduli, nslots, obf->rng);
            encode_v_ib(obf->mmap, obf->R_ib[i][b], obf->sp, tmp, i, b);
            /* Compute Z_i,b encodings */
            if (!obf->op->simple)
                mpz_urandomm_vect_aes(other, moduli, nslots, obf->rng);
            mpz_set(   other[0], ys[i]);
            mpz_set_ui(other[1], b);
            mpz_vect_mul_mod(tmp, tmp, other, moduli, nslots);
            encode_v_ib_v_star(obf->mmap, obf->Z_ib[i][b], obf->sp, tmp, i, b);
            mpz_vect_clear(tmp, nslots);
            mpz_vect_clear(other, nslots);
        }
    }

    /* encode R_hat_ib and Z_hat_ib */
    for (size_t o = 0; o < gamma; o++) {
        for (size_t i = 0; i < n; i++) {
            for (size_t b = 0; b < 2; b++) {
                mpz_t tmp[nslots];
                mpz_vect_init(tmp, nslots);
                /* Compute R_hat_i,b encodings */
                mpz_urandomm_vect_aes(tmp, moduli, nslots, obf->rng);
                encode_v_hat_ib_o(obf->mmap, obf->R_hat_ib_o[o][i][b], obf->sp, tmp, i, b, o);
                /* Compute Z_hat_i,b encodings */
                mpz_vect_mul_mod(tmp, tmp, w_hats[i], moduli, nslots);
                encode_v_hat_ib_o_v_star(obf->mmap, obf->Z_hat_ib_o[o][i][b], obf->sp, tmp, i, b, o);
                mpz_vect_clear(tmp, nslots);
            }
        }
    }

    /* encode R_i and Z_i */
    assert(obf->op->circ->nconsts == m);
    for (size_t i = 0; i < m; i++) {
        mpz_t tmp[nslots], other[nslots];
        mpz_vect_init(tmp, nslots);
        mpz_vect_init(other, nslots);
        /* Compute R_i encodings */
        mpz_urandomm_vect_aes(tmp, moduli, nslots, obf->rng);
        encode_v_i(obf->mmap, obf->R_i[i], obf->sp, tmp, i);
        /* Compute Z_i encodings */
        mpz_urandomm_vect_aes(other, moduli, nslots, obf->rng);
        mpz_set(   other[0], ys[n + i]);
        mpz_set_ui(other[1], obf->op->circ->consts[i]);
        mpz_vect_mul_mod(tmp, tmp, other, moduli, nslots);
        encode_v_i_v_star(obf->mmap, obf->Z_i[i], obf->sp, tmp, i);
        mpz_vect_clear(tmp, nslots);
        mpz_vect_clear(other, nslots);
    }

    /* encode R_o_i and Z_o_i */
    for (size_t i = 0; i < gamma; ++i) {
        mpz_t rs[nslots], ws[nslots];
        mpz_vect_init(rs, nslots);
        mpz_vect_init(ws, nslots);
        /* Compute R_o_i encodings */
        mpz_urandomm_vect_aes(rs, moduli, nslots, obf->rng);
        encode_v_hat_o(obf->mmap, obf->R_o_i[i], obf->sp, rs, i);
        /* Compute Z_o_i encodings */
        acirc_eval_mpz_mod(ws[0], obf->op->circ, obf->op->circ->outrefs[i], ys,
                           ys + n, moduli[0]);
        mpz_set_ui(ws[1], 1);
        for (size_t j = 0; j < n; ++j) {
            mpz_vect_mul_mod(rs, rs, w_hats[j], moduli, nslots);
        }
        mpz_vect_mul_mod(rs, rs, ws, moduli, nslots);
        encode_v_hat_o_v_star(obf->mmap, obf->Z_o_i[i], obf->sp, rs, i);
        mpz_vect_clear(rs, nslots);
        mpz_vect_clear(ws, nslots);
    }

    for (size_t i = 0; i < n; ++i) {
        mpz_vect_clear(w_hats[i], nslots);
    }
    mpz_vect_clear(ys, n + m);
    mpz_vect_destroy(moduli, obf->mmap->sk->nslots(obf->sp->sk));
}

void
obfuscation_fwrite(const obfuscation *const obf, FILE *const fp)
{
    const obf_params_t *op = obf->pp->op;

    public_params_fwrite(obf->mmap, obf->pp, fp);
    PUT_NEWLINE(fp);

    for (size_t i = 0; i < op->n; i++) {
        for (size_t b = 0; b < 2; b++) {
            encoding_fwrite(obf->mmap, obf->R_ib[i][b], fp);
            PUT_NEWLINE(fp);
        }
    }
    for (size_t i = 0; i < op->n; i++) {
        for (size_t b = 0; b < 2; b++) {
            encoding_fwrite(obf->mmap, obf->Z_ib[i][b], fp);
            PUT_NEWLINE(fp);
        }
    }
    for (size_t o = 0; o < op->gamma; o++) {
        for (size_t i = 0; i < op->n; i++) {
            for (size_t b = 0; b < 2; b++) {
                encoding_fwrite(obf->mmap, obf->R_hat_ib_o[o][i][b], fp);
                PUT_NEWLINE(fp);
            }
        }
    }
    for (size_t o = 0; o < op->gamma; o++) {
        for (size_t i = 0; i < op->n; i++) {
            for (size_t b = 0; b < 2; b++) {
                encoding_fwrite(obf->mmap, obf->Z_hat_ib_o[o][i][b], fp);
                PUT_NEWLINE(fp);
            }
        }
    }
    for (size_t i = 0; i < op->m; i++) {
        encoding_fwrite(obf->mmap, obf->R_i[i], fp);
        PUT_NEWLINE(fp);
    }
    for (size_t i = 0; i < op->m; i++) {
        encoding_fwrite(obf->mmap, obf->Z_i[i], fp);
        PUT_NEWLINE(fp);
    }
    for (size_t i = 0; i < op->gamma; i++) {
        encoding_fwrite(obf->mmap, obf->R_o_i[i], fp);
        PUT_NEWLINE(fp);
    }
    for (size_t i = 0; i < op->gamma; i++) {
        encoding_fwrite(obf->mmap, obf->Z_o_i[i], fp);
        PUT_NEWLINE(fp);
    }
}

obfuscation *
obfuscation_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *const fp)
{
    obfuscation *obf;

    obf = calloc(1, sizeof(obfuscation));
    if (obf == NULL)
        return NULL;

    obf->mmap = mmap;
    obf->op = op;
    obf->sp = NULL;
    obf->pp = calloc(1, sizeof(public_params));
    (void) public_params_fread(obf->mmap, obf->pp, op, fp);
    GET_NEWLINE(fp);

    obf->R_ib = lin_calloc(op->n, sizeof(encoding **));
    for (size_t i = 0; i < op->n; i++) {
        obf->R_ib[i] = lin_calloc(2, sizeof(encoding *));
        for (size_t b = 0; b < 2; b++) {
            obf->R_ib[i][b] = lin_calloc(1, sizeof(encoding));
            encoding_fread(obf->mmap, obf->R_ib[i][b], fp);
            GET_NEWLINE(fp);
        }
    }
    obf->Z_ib = lin_calloc(op->n, sizeof(encoding **));
    for (size_t i = 0; i < op->n; i++) {
        obf->Z_ib[i] = lin_calloc(2, sizeof(encoding *));
        for (size_t b = 0; b < 2; b++) {
            obf->Z_ib[i][b] = lin_calloc(1, sizeof(encoding));
            encoding_fread(obf->mmap, obf->Z_ib[i][b], fp);
            GET_NEWLINE(fp);
        }
    }
    obf->R_hat_ib_o = lin_calloc(op->gamma, sizeof(encoding ***));
    for (size_t o = 0; o < op->gamma; o++) {
        obf->R_hat_ib_o[o] = lin_calloc(op->n, sizeof(encoding **));
        for (size_t i = 0; i < op->n; i++) {
            obf->R_hat_ib_o[o][i] = lin_calloc(2, sizeof(encoding *));
            for (size_t b = 0; b < 2; b++) {
                obf->R_hat_ib_o[o][i][b] = lin_calloc(1, sizeof(encoding));
                encoding_fread(obf->mmap, obf->R_hat_ib_o[o][i][b], fp);
                GET_NEWLINE(fp);
            }
        }
    }
    obf->Z_hat_ib_o = lin_calloc(op->gamma, sizeof(encoding ***));
    for (size_t o = 0; o < op->gamma; o++) {
        obf->Z_hat_ib_o[o] = lin_calloc(op->n, sizeof(encoding **));
        for (size_t i = 0; i < op->n; i++) {
            obf->Z_hat_ib_o[o][i] = lin_calloc(2, sizeof(encoding *));
            for (size_t b = 0; b < 2; b++) {
                obf->Z_hat_ib_o[o][i][b] = lin_calloc(1, sizeof(encoding));
                encoding_fread(obf->mmap, obf->Z_hat_ib_o[o][i][b], fp);
                GET_NEWLINE(fp);
            }
        }
    }
    obf->R_i = lin_malloc(op->m * sizeof(encoding *));
    for (size_t i = 0; i < op->m; i++) {
        obf->R_i[i] = lin_calloc(1, sizeof(encoding));
        encoding_fread(obf->mmap, obf->R_i[i], fp);
        GET_NEWLINE(fp);
    }
    obf->Z_i = lin_malloc(op->m * sizeof(encoding *));
    for (size_t i = 0; i < op->m; i++) {
        obf->Z_i[i] = lin_calloc(1, sizeof(encoding));
        encoding_fread(obf->mmap, obf->Z_i[i], fp);
        GET_NEWLINE(fp);
    }
    obf->R_o_i = lin_malloc(op->gamma * sizeof(encoding *));
    for (size_t i = 0; i < op->gamma; i++) {
        obf->R_o_i[i] = lin_calloc(1, sizeof(encoding));
        encoding_fread(obf->mmap, obf->R_o_i[i], fp);
        GET_NEWLINE(fp);
    }
    obf->Z_o_i = lin_malloc(op->gamma * sizeof(encoding *));
    for (size_t i = 0; i < op->gamma; i++) {
        obf->Z_o_i[i] = lin_calloc(1, sizeof(encoding));
        encoding_fread(obf->mmap, obf->Z_o_i[i], fp);
        GET_NEWLINE(fp);
    }

    return obf;
}

typedef struct {
    encoding *r;
    encoding *z;
    bool my_r; // whether this wire "owns" r
    bool my_z; // whether this wire "owns" z
} wire;

static void
wire_print(wire *w)
{
    printf("r:\n");
    level_print(w->r->lvl);
    printf("z:\n");
    level_print(w->z->lvl);
}

static void
wire_init(const mmap_vtable *mmap, wire *rop, public_params *pp, bool init_r,
          bool init_z)
{
    if (init_r) {
        rop->r = encoding_new(mmap, pp);
    }
    if (init_z) {
        rop->z = encoding_new(mmap, pp);
    }
    rop->my_r = init_r;
    rop->my_z = init_z;
}

static void
wire_init_from_encodings(wire *rop, encoding *r, encoding *z)
{
    rop->r = r;
    rop->z = z;
    rop->my_r = false;
    rop->my_z = false;
}

static void
wire_clear(const mmap_vtable *mmap, wire *rop)
{
    if (rop->my_r) {
        encoding_free(mmap, rop->r);
    }
    if (rop->my_z) {
        encoding_free(mmap, rop->z);
    }
}

static void
wire_copy(const mmap_vtable *mmap, wire *rop, wire *source, public_params *p)
{
    rop->r = encoding_new(mmap, p);
    encoding_set(mmap, rop->r, source->r);
    rop->my_r = 1;

    rop->z = encoding_new(mmap, p);
    encoding_set(mmap, rop->z, source->z);
    rop->my_z = 1;
}

static void
wire_mul(const mmap_vtable *mmap, wire *rop, wire *x, wire *y, public_params *p)
{
    encoding_mul(mmap, rop->r, x->r, y->r, p);
    encoding_mul(mmap, rop->z, x->z, y->z, p);
}

static void
wire_add(const mmap_vtable *mmap, wire *rop, wire *x, wire *y, public_params *p)
{
    encoding *tmp;
    encoding_mul(mmap, rop->r, x->r, y->r, p);
    tmp = encoding_new(mmap, p);
    encoding_mul(mmap, rop->z, x->z, y->r, p);
    encoding_mul(mmap, tmp, y->z, x->r, p);
    encoding_add(mmap, rop->z, rop->z, tmp, p);
    encoding_free(mmap, tmp);
}

static void
wire_sub(const mmap_vtable *mmap, wire *rop, wire *x, wire *y, public_params *p)
{
    encoding *tmp;
    encoding_mul(mmap, rop->r, x->r, y->r, p);
    tmp = encoding_new(mmap, p);
    encoding_mul(mmap, rop->z, x->z, y->r, p);
    encoding_mul(mmap, tmp, y->z, x->r, p);
    encoding_sub(mmap, rop->z, rop->z, tmp, p);
    encoding_free(mmap, tmp);
}

static int
wire_type_eq(wire *x, wire *y)
{
    if (!level_eq(x->r->lvl, y->r->lvl))
        return 0;
    if (!level_eq(x->z->lvl, y->z->lvl))
        return 0;
    return 1;
}

void
obfuscation_eval(const mmap_vtable *mmap, int *rop, const int *inps,
                 const obfuscation *const obf)
{
    const obf_params_t *op = obf->op;
    public_params *pp = obf->pp;
    const acirc *c = op->circ;
    int known[c->nrefs];
    wire *cache[c->nrefs];
    int input_syms[op->n];

    for (size_t i = 0; i < op->n; i++) {
        input_syms[i] = 0;
        for (size_t j = 0; j < 1; j++) {
            sym_id sym = { i, j };
            acircref inp_bit = op->rchunker(sym, c->ninputs, c->ninputs);
            input_syms[i] += inps[inp_bit] << j;
        }
    }

    memset(known, '\0', sizeof(known));
    memset(cache, '\0', sizeof(known));

    for (size_t o = 0; o < c->noutputs; o++) {
        acircref root = c->outrefs[o];
        acirc_topo_levels *topo = acirc_topological_levels(c, root);
        for (int lvl = 0; lvl < topo->nlevels; lvl++) {
            for (int i = 0; i < topo->level_sizes[lvl]; i++) {
                acircref ref = topo->levels[lvl][i];
                if (known[ref])
                    continue;

                acirc_operation aop = c->gates[ref].op;
                acircref *args = c->gates[ref].args;
                wire *w = lin_malloc(sizeof(wire));
                if (aop == XINPUT) {
                    size_t xid = args[0];
                    sym_id sym = op->chunker(xid, c->ninputs, op->n);
                    int k = sym.sym_number;
                    int s = input_syms[k];
                    wire_init_from_encodings(w, obf->R_ib[xid][s], obf->Z_ib[xid][s]);
                    /* printf("XINPUT\n"); */
                    /* obf->mmap->enc->print(&obf->R_ib[xid][s]->enc); */
                    /* obf->mmap->enc->print(&obf->Z_ib[xid][s]->enc); */
                    /* wire_print(w); */
                } else if (aop == YINPUT) {
                    size_t yid = args[0];
                    wire_init_from_encodings(w, obf->R_i[yid], obf->Z_i[yid]);
                    /* printf("YINPUT\n"); */
                    /* obf->mmap->enc->print(&obf->R_i[yid]->enc); */
                    /* obf->mmap->enc->print(&obf->Z_i[yid]->enc); */
                    /* wire_print(w); */
                } else {
                    wire *x = cache[args[0]];
                    wire *y = cache[args[1]];

                    wire_init(mmap, w, pp, true, true);

                    if (aop == MUL) {
                        wire_mul(mmap, w, x, y, pp);
                    } else if (aop == ADD) {
                        wire_add(mmap, w, x, y, pp);
                    } else if (aop == SUB) {
                        wire_sub(mmap, w, x, y, pp);
                    }
                    /* printf("OP\n"); */
                    /* obf->mmap->enc->print(&x->r->enc); */
                    /* obf->mmap->enc->print(&x->z->enc); */

                    /* obf->mmap->enc->print(&y->r->enc); */
                    /* obf->mmap->enc->print(&y->z->enc); */

                    /* obf->mmap->enc->print(&w->r->enc); */
                    /* obf->mmap->enc->print(&w->z->enc); */

                    /* printf("OP\n"); */
                    /* wire_print(x); */
                    /* wire_print(y); */
                    /* wire_print(w); */
                }

                known[ref] = true;
                cache[ref] = w;
            }
        }
        acirc_topo_levels_destroy(topo);

        wire tmp[1];
        wire tmp2[1];
        wire outwire[1];

        wire_copy(mmap, outwire, cache[root], pp);

        for (size_t k = 0; k < op->n; k++) {
            wire_init_from_encodings(tmp,
                                     obf->R_hat_ib_o[o][k][input_syms[k]],
                                     obf->Z_hat_ib_o[o][k][input_syms[k]]);
            /* printf("FINAL MULS!\n"); */
            /* obf->mmap->enc->print(&outwire->r->enc); */
            /* obf->mmap->enc->print(&outwire->z->enc); */
            /* obf->mmap->enc->print(&tmp->r->enc); */
            /* obf->mmap->enc->print(&tmp->z->enc); */
            /* wire_print(tmp); */
            /* wire_print(outwire); */
            wire_mul(mmap, outwire, outwire, tmp, pp);
            /* obf->mmap->enc->print(&outwire->r->enc); */
            /* obf->mmap->enc->print(&outwire->z->enc); */
        }

        wire_copy(mmap, tmp2, outwire, pp);
        wire_init_from_encodings(tmp, obf->R_o_i[o], obf->Z_o_i[o]);
        
        /* printf("FINAL SUB!\n"); */
        /* obf->mmap->enc->print(&tmp2->r->enc); */
        /* obf->mmap->enc->print(&tmp2->z->enc); */
        /* obf->mmap->enc->print(&tmp->r->enc); */
        /* obf->mmap->enc->print(&tmp->z->enc); */

        wire_sub(mmap, outwire, tmp2, tmp, pp);

        /* obf->mmap->enc->print(&outwire->r->enc); */
        /* obf->mmap->enc->print(&outwire->z->enc); */
        
        /* wire_print(tmp); */
        /* wire_print(tmp2); */
        /* wire_print(outwire); */
        rop[o] = encoding_is_zero(mmap, outwire->z, pp);

        wire_clear(mmap, outwire);
        wire_clear(mmap, tmp);
        wire_clear(mmap, tmp2);
    }

    for (acircref x = 0; x < c->nrefs; x++) {
        if (known[x]) {
            wire_clear(mmap, cache[x]);
            free(cache[x]);
        }
    }
}
