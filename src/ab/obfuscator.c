#include "vtables.h"
#include "obfuscator.h"
#include "obf_params.h"

#include "../util.h"

#include <assert.h>
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct obfuscation {
    const mmap_vtable *mmap;
    const pp_vtable *pp_vt;
    const sp_vtable *sp_vt;
    const encoding_vtable *enc_vt;
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
encode_v_ib(const encoding_vtable *vt, const obf_params_t *op,
            encoding *enc, const secret_params *sp,
            mpz_t *rs, size_t i, bool b)
{
    level *const lvl = level_create_v_ib(op, i, b);
    encode(vt, enc, rs, op->nslots, lvl, sp);
    level_free(lvl);
}

static void
encode_v_ib_v_star(const encoding_vtable *vt, const obf_params_t *op,
                   encoding *enc, const secret_params *sp,
                   mpz_t *rs, size_t i, bool b)
{
    level *const lvl = level_create_v_ib(op, i, b);
    level *const v_star = level_create_v_star(op);
    level_add(lvl, lvl, v_star);
    encode(vt, enc, rs, op->nslots, lvl, sp);
    level_free(v_star);
    level_free(lvl);
}

static void
encode_v_hat_ib_o(const encoding_vtable *vt, const obf_params_t *op,
                  encoding *enc, const secret_params *sp,
                  mpz_t *rs, size_t i, bool b, size_t o)
{
    level *const lvl = level_create_v_hat_ib_o(op, i, b, o);
    encode(vt, enc, rs, op->nslots, lvl, sp);
    level_free(lvl);
}

static void
encode_v_hat_ib_o_v_star(const encoding_vtable *vt,
                         const obf_params_t *op, encoding *enc,
                         const secret_params *sp, mpz_t *rs,
                         size_t i, bool b, size_t o)
{
    level *const lvl = level_create_v_hat_ib_o(op, i, b, o);
    level *const v_star = level_create_v_star(op);
    level_add(lvl, lvl, v_star);
    encode(vt, enc, rs, op->nslots, lvl, sp);
    level_free(v_star);
    level_free(lvl);
}

static void
encode_v_i(const encoding_vtable *vt, const obf_params_t *op,
           encoding *enc, const secret_params *sp,
           mpz_t *rs, size_t i)
{
    level *const lvl = level_create_v_i(op, op->n + i);
    encode(vt, enc, rs, op->nslots, lvl, sp);
    level_free(lvl);
}

static void
encode_v_i_v_star(const encoding_vtable *vt, const obf_params_t *op,
                  encoding *enc, const secret_params *sp,
                  mpz_t *rs, size_t i)
{
    level *const lvl = level_create_v_i(op, op->n + i);
    level *const v_star = level_create_v_star(op);
    level_add(lvl, lvl, v_star);
    level_free(v_star);
    encode(vt, enc, rs, op->nslots, lvl, sp);
    level_free(lvl);
}

static void
encode_v_0(const encoding_vtable *vt, const obf_params_t *op,
           encoding *enc, const secret_params *sp,
           mpz_t *rs, size_t o)
{
    (void) o;
    level *const lvl = level_create_v_0(op);
    encode(vt, enc, rs, op->nslots, lvl, sp);
    level_free(lvl);
}

static void
encode_v_0_v_star(const encoding_vtable *vt, const obf_params_t *op,
                  encoding *enc, const secret_params *sp, mpz_t *rs, size_t o)
{
    (void) o;
    level *const lvl = level_create_v_0(op);
    level *const v_star = level_create_v_star(op);
    level_mul_ui(v_star, v_star, op->D);
    level_add(lvl, lvl, v_star);
    encode(vt, enc, rs, op->nslots, lvl, sp);
    level_free(v_star);
    level_free(lvl);
}

static void
_obfuscation_free(obfuscation *obf);

static obfuscation *
_obfuscation_new(const mmap_vtable *mmap, const obf_params_t *op,
                 size_t secparam, size_t kappa)
{
    obfuscation *obf;

    if (op->gamma > 1) {
        errx(1, "Applebaum-Brakerski currently only supports 1 output bit");
    }

    obf = my_calloc(1, sizeof(obfuscation));
    obf->mmap = mmap;
    obf->enc_vt = ab_get_encoding_vtable(mmap);
    obf->pp_vt = ab_get_pp_vtable(mmap);
    obf->sp_vt = ab_get_sp_vtable(mmap);
    obf->op = op;
    aes_randinit(obf->rng);
    obf->sp = secret_params_new(obf->sp_vt, op, secparam, kappa, obf->rng);
    obf->pp = public_params_new(obf->pp_vt, obf->sp_vt, obf->sp);

    obf->R_ib = my_calloc(op->n, sizeof(encoding ***));
    for (size_t i = 0; i < op->n; i++) {
        obf->R_ib[i] = my_calloc(2, sizeof(encoding **));
        for (size_t b = 0; b < 2; b++) {
            obf->R_ib[i][b] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        }
    }
    obf->Z_ib = my_calloc(op->n, sizeof(encoding ***));
    for (size_t i = 0; i < op->n; i++) {
        obf->Z_ib[i] = my_calloc(2, sizeof(encoding **));
        for (size_t b = 0; b < 2; b++) {
            obf->Z_ib[i][b] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        }
    }
    obf->R_hat_ib_o = my_calloc(op->gamma, sizeof(encoding ***));
    for (size_t o = 0; o < op->gamma; ++o) {
        obf->R_hat_ib_o[o] = my_calloc(op->n, sizeof(encoding **));
        for (size_t i = 0; i < op->n; i++) {
            obf->R_hat_ib_o[o][i] = my_calloc(2, sizeof(encoding *));
            for (size_t b = 0; b < 2; b++) {
                obf->R_hat_ib_o[o][i][b] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
        }
    }
    obf->Z_hat_ib_o = my_calloc(op->gamma, sizeof(encoding ***));
    for (size_t o = 0; o < op->gamma; ++o) {
        obf->Z_hat_ib_o[o] = my_calloc(op->n, sizeof(encoding **));
        for (size_t i = 0; i < op->n; i++) {
            obf->Z_hat_ib_o[o][i] = my_calloc(2, sizeof(encoding *));
            for (size_t b = 0; b < 2; b++) {
                obf->Z_hat_ib_o[o][i][b] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
        }
    }
    obf->R_i = my_calloc(op->m, sizeof(encoding *));
    for (size_t i = 0; i < op->m; i++) {
        obf->R_i[i] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }
    obf->Z_i = my_calloc(op->m, sizeof(encoding *));
    for (size_t i = 0; i < op->m; i++) {
        obf->Z_i[i] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }
    obf->R_o_i = my_calloc(op->gamma, sizeof(encoding *));
    for (size_t i = 0; i < op->gamma; i++) {
        obf->R_o_i[i] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }
    obf->Z_o_i = my_calloc(op->gamma, sizeof(encoding *));
    for (size_t i = 0; i < op->gamma; i++) {
        obf->Z_o_i[i] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }

    return obf;
}

static void
_obfuscation_free(obfuscation *obf)
{
    if (obf == NULL)
        return;

    const obf_params_t *op = obf->op;

    if (obf->R_ib) {
        for (size_t i = 0; i < op->n; i++) {
            for (size_t b = 0; b < 2; b++) {
                encoding_free(obf->enc_vt, obf->R_ib[i][b]);
            }
            free(obf->R_ib[i]);
        }
        free(obf->R_ib);
    }

    if (obf->Z_ib) {
        for (size_t i = 0; i < op->n; i++) {
            for (size_t b = 0; b < 2; b++) {
                encoding_free(obf->enc_vt, obf->Z_ib[i][b]);
            }
            free(obf->Z_ib[i]);
        }
        free(obf->Z_ib);
    }
    if (obf->R_hat_ib_o) {
        for (size_t o = 0; o < op->gamma; ++o) {
            for (size_t i = 0; i < op->n; i++) {
                for (size_t b = 0; b < 2; b++) {
                    encoding_free(obf->enc_vt, obf->R_hat_ib_o[o][i][b]);
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
                    encoding_free(obf->enc_vt, obf->Z_hat_ib_o[o][i][b]);
                }
                free(obf->Z_hat_ib_o[o][i]);
            }
            free(obf->Z_hat_ib_o[o]);
        }
        free(obf->Z_hat_ib_o);
    }
    if (obf->R_i) {
        for (size_t i = 0; i < op->m; i++) {
            encoding_free(obf->enc_vt, obf->R_i[i]);
        }
        free(obf->R_i);
    }
    if (obf->Z_i) {
        for (size_t i = 0; i < op->m; i++) {
            encoding_free(obf->enc_vt, obf->Z_i[i]);
        }
        free(obf->Z_i);
    }
    if (obf->R_o_i) {
        for (size_t i = 0; i < op->gamma; i++) {
            encoding_free(obf->enc_vt, obf->R_o_i[i]);
        }
        free(obf->R_o_i);
    }
    if (obf->Z_o_i) {
        for (size_t i = 0; i < op->gamma; i++) {
            encoding_free(obf->enc_vt, obf->Z_o_i[i]);
        }
        free(obf->Z_o_i);
    }
    if (obf->pp)
        public_params_free(obf->pp_vt, obf->pp);
    if (obf->sp)
        secret_params_free(obf->sp_vt, obf->sp);
    if (obf->rng)
        aes_randclear(obf->rng);
    free(obf);
}

static int
_obfuscate(obfuscation *obf, size_t nthreads)
{
    (void) nthreads;
    mpz_t *moduli =
        mpz_vect_create_of_fmpz(obf->mmap->sk->plaintext_fields(obf->sp->sk),
                                obf->mmap->sk->nslots(obf->sp->sk));
    const bool simple = obf->op->simple;
    const size_t n = obf->op->n,
                 m = obf->op->m,
                 gamma = obf->op->gamma,
                 nslots = simple ? 2 : (n + 2);
    mpz_t ys[n + m], w_hats[n][nslots];

    mpz_vect_init(ys, n + m);
    mpz_vect_urandomm(ys, moduli[0], n + m, obf->rng);

    if (LOG_DEBUG) {
        for (size_t i = 0; i < nslots; ++i) {
            gmp_fprintf(stderr, "%Zd\t", moduli[i]);
        }
        fprintf(stderr, "\n");
    }

    for (size_t i = 0; i < n; ++i) {
        mpz_vect_init(w_hats[i], nslots);
        mpz_vect_urandomms(w_hats[i], (const mpz_t *) moduli, nslots, obf->rng);
        if (!simple) {
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
            mpz_vect_urandomms(tmp, (const mpz_t *) moduli, nslots, obf->rng);
            encode_v_ib(obf->enc_vt, obf->op, obf->R_ib[i][b], obf->sp, tmp, i, b);
            if (LOG_DEBUG) {
                fprintf(stderr, "R_%lu,%lu --------\n", i, b);
                encoding_print(obf->enc_vt, obf->R_ib[i][b]);
                fprintf(stderr, "--------------\n");
            }
            /* Compute Z_i,b encodings */
            if (!simple)
                mpz_vect_urandomms(other, (const mpz_t *) moduli, nslots, obf->rng);
            mpz_set   (other[0], ys[i]);
            mpz_set_ui(other[1], b);
            mpz_vect_mul_mod(tmp, (const mpz_t *) tmp, (const mpz_t *) other, (const mpz_t *) moduli, nslots);
            encode_v_ib_v_star(obf->enc_vt, obf->op, obf->Z_ib[i][b], obf->sp, tmp, i, b);
            if (LOG_DEBUG) {
                printf("Z_%lu,%lu --------\n", i, b);
                encoding_print(obf->enc_vt, obf->Z_ib[i][b]);
                printf("--------------\n");
            }
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
                mpz_vect_urandomms(tmp, (const mpz_t *) moduli, nslots, obf->rng);
                encode_v_hat_ib_o(obf->enc_vt, obf->op, obf->R_hat_ib_o[o][i][b], obf->sp, tmp, i, b, o);
                if (LOG_DEBUG) {
                    printf("R_hat_%lu,%lu --------\n", i, b);
                    encoding_print(obf->enc_vt, obf->R_hat_ib_o[o][i][b]);
                    printf("------------------\n");
                }
                /* Compute Z_hat_i,b encodings */
                mpz_vect_mul_mod(tmp, (const mpz_t *) tmp, (const mpz_t *) w_hats[i], (const mpz_t *) moduli, nslots);
                encode_v_hat_ib_o_v_star(obf->enc_vt, obf->op, obf->Z_hat_ib_o[o][i][b], obf->sp, tmp, i, b, o);
                if (LOG_DEBUG) {
                    printf("Z_hat_%lu,%lu --------\n", i, b);
                    encoding_print(obf->enc_vt, obf->Z_hat_ib_o[o][i][b]);
                    printf("------------------\n");
                }
                mpz_vect_clear(tmp, nslots);
            }
        }
    }

    /* encode R_i and Z_i */
    for (size_t i = 0; i < m; i++) {
        mpz_t tmp[nslots], other[nslots];
        mpz_vect_init(tmp, nslots);
        mpz_vect_init(other, nslots);
        /* Compute R_i encodings */
        mpz_vect_urandomms(tmp, (const mpz_t *) moduli, nslots, obf->rng);
        encode_v_i(obf->enc_vt, obf->op, obf->R_i[i], obf->sp, tmp, i);
        if (LOG_DEBUG) {
            printf("R_%lu --------\n", i);
            encoding_print(obf->enc_vt, obf->R_i[i]);
            printf("------------\n");
        }
        /* Compute Z_i encodings */
        mpz_vect_urandomms(other, (const mpz_t *) moduli, nslots, obf->rng);
        mpz_set(   other[0], ys[n + i]);
        mpz_set_ui(other[1], obf->op->circ->consts[i]);
        mpz_vect_mul_mod(tmp, (const mpz_t *) tmp, (const mpz_t *) other, (const mpz_t *) moduli, nslots);
        encode_v_i_v_star(obf->enc_vt, obf->op, obf->Z_i[i], obf->sp, tmp, i);
        if (LOG_DEBUG) {
            printf("Z_%lu --------\n", i);
            encoding_print(obf->enc_vt, obf->Z_i[i]);
            printf("------------\n");
        }
        mpz_vect_clear(tmp, nslots);
        mpz_vect_clear(other, nslots);
    }

    /* encode R_o_i and Z_o_i */
    for (size_t o = 0; o < gamma; ++o) {
        mpz_t rs[nslots], ws[nslots];
        mpz_vect_init(rs, nslots);
        mpz_vect_init(ws, nslots);
        /* Compute R_o_i encodings */
        mpz_vect_urandomms(rs, (const mpz_t *) moduli, nslots, obf->rng);
        encode_v_0(obf->enc_vt, obf->op, obf->R_o_i[o], obf->sp, rs, o);
        if (LOG_DEBUG) {
            printf("R_0 --------\n");
            encoding_print(obf->enc_vt, obf->R_o_i[o]);
            printf("------------\n");
        }
        /* Compute Z_o_i encodings */
        acirc_eval_mpz_mod(ws[0], obf->op->circ, obf->op->circ->outrefs[o], (const mpz_t *) ys,
                           (const mpz_t *) ys + n, moduli[0]);
        mpz_set_ui(ws[1], 1);
        for (size_t i = 2; i < nslots; ++i) {
            mpz_set_ui(ws[i], 0);
        }
        for (size_t i = 0; i < n; ++i) {
            mpz_vect_mul_mod(rs, (const mpz_t *) rs, (const mpz_t *) w_hats[i], (const mpz_t *) moduli, nslots);
        }
        mpz_vect_mul_mod(rs, (const mpz_t *) rs, (const mpz_t *) ws, (const mpz_t *) moduli, nslots);
        encode_v_0_v_star(obf->enc_vt, obf->op, obf->Z_o_i[o], obf->sp, rs, o);
        if (LOG_DEBUG) {
            printf("Z_0 --------\n");
            encoding_print(obf->enc_vt, obf->Z_o_i[o]);
            printf("------------\n");
        }
        mpz_vect_clear(rs, nslots);
        mpz_vect_clear(ws, nslots);
    }

    for (size_t i = 0; i < n; ++i) {
        mpz_vect_clear(w_hats[i], nslots);
    }
    mpz_vect_clear(ys, n + m);
    mpz_vect_free(moduli, obf->mmap->sk->nslots(obf->sp->sk));

    return OK;
}

static int
_obfuscation_fwrite(const obfuscation *obf, FILE *fp)
{
    const obf_params_t *op = obf->op;

    public_params_fwrite(obf->pp_vt, obf->pp, fp);

    for (size_t i = 0; i < op->n; i++) {
        for (size_t b = 0; b < 2; b++) {
            encoding_fwrite(obf->enc_vt, obf->R_ib[i][b], fp);
        }
    }
    for (size_t i = 0; i < op->n; i++) {
        for (size_t b = 0; b < 2; b++) {
            encoding_fwrite(obf->enc_vt, obf->Z_ib[i][b], fp);
        }
    }
    for (size_t o = 0; o < op->gamma; o++) {
        for (size_t i = 0; i < op->n; i++) {
            for (size_t b = 0; b < 2; b++) {
                encoding_fwrite(obf->enc_vt, obf->R_hat_ib_o[o][i][b], fp);
            }
        }
    }
    for (size_t o = 0; o < op->gamma; o++) {
        for (size_t i = 0; i < op->n; i++) {
            for (size_t b = 0; b < 2; b++) {
                encoding_fwrite(obf->enc_vt, obf->Z_hat_ib_o[o][i][b], fp);
            }
        }
    }
    for (size_t i = 0; i < op->m; i++) {
        encoding_fwrite(obf->enc_vt, obf->R_i[i], fp);
    }
    for (size_t i = 0; i < op->m; i++) {
        encoding_fwrite(obf->enc_vt, obf->Z_i[i], fp);
    }
    for (size_t i = 0; i < op->gamma; i++) {
        encoding_fwrite(obf->enc_vt, obf->R_o_i[i], fp);
    }
    for (size_t i = 0; i < op->gamma; i++) {
        encoding_fwrite(obf->enc_vt, obf->Z_o_i[i], fp);
    }

    return OK;
}

static obfuscation *
_obfuscation_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp)
{
    obfuscation *obf;

    obf = my_calloc(1, sizeof(obfuscation));
    if (obf == NULL)
        return NULL;

    obf->mmap = mmap;
    obf->pp_vt = ab_get_pp_vtable(mmap);
    obf->sp_vt = ab_get_sp_vtable(mmap);
    obf->enc_vt = ab_get_encoding_vtable(mmap);
    obf->op = op;
    obf->sp = NULL;
    obf->pp = public_params_fread(obf->pp_vt, op, fp);

    obf->R_ib = my_calloc(op->n, sizeof(encoding **));
    for (size_t i = 0; i < op->n; i++) {
        obf->R_ib[i] = my_calloc(2, sizeof(encoding *));
        for (size_t b = 0; b < 2; b++) {
            obf->R_ib[i][b] = encoding_fread(obf->enc_vt, fp);
        }
    }
    obf->Z_ib = my_calloc(op->n, sizeof(encoding **));
    for (size_t i = 0; i < op->n; i++) {
        obf->Z_ib[i] = my_calloc(2, sizeof(encoding *));
        for (size_t b = 0; b < 2; b++) {
            obf->Z_ib[i][b] = encoding_fread(obf->enc_vt, fp);
        }
    }
    obf->R_hat_ib_o = my_calloc(op->gamma, sizeof(encoding ***));
    for (size_t o = 0; o < op->gamma; o++) {
        obf->R_hat_ib_o[o] = my_calloc(op->n, sizeof(encoding **));
        for (size_t i = 0; i < op->n; i++) {
            obf->R_hat_ib_o[o][i] = my_calloc(2, sizeof(encoding *));
            for (size_t b = 0; b < 2; b++) {
                obf->R_hat_ib_o[o][i][b] = encoding_fread(obf->enc_vt, fp);
            }
        }
    }
    obf->Z_hat_ib_o = my_calloc(op->gamma, sizeof(encoding ***));
    for (size_t o = 0; o < op->gamma; o++) {
        obf->Z_hat_ib_o[o] = my_calloc(op->n, sizeof(encoding **));
        for (size_t i = 0; i < op->n; i++) {
            obf->Z_hat_ib_o[o][i] = my_calloc(2, sizeof(encoding *));
            for (size_t b = 0; b < 2; b++) {
                obf->Z_hat_ib_o[o][i][b] = encoding_fread(obf->enc_vt, fp);
            }
        }
    }
    obf->R_i = my_malloc(op->m * sizeof(encoding *));
    for (size_t i = 0; i < op->m; i++) {
        obf->R_i[i] = encoding_fread(obf->enc_vt, fp);
    }
    obf->Z_i = my_malloc(op->m * sizeof(encoding *));
    for (size_t i = 0; i < op->m; i++) {
        obf->Z_i[i] = encoding_fread(obf->enc_vt, fp);
    }
    obf->R_o_i = my_malloc(op->gamma * sizeof(encoding *));
    for (size_t i = 0; i < op->gamma; i++) {
        obf->R_o_i[i] = encoding_fread(obf->enc_vt, fp);
    }
    obf->Z_o_i = my_malloc(op->gamma * sizeof(encoding *));
    for (size_t i = 0; i < op->gamma; i++) {
        obf->Z_o_i[i] = encoding_fread(obf->enc_vt, fp);
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
wire_print(const encoding_vtable *vt, const wire *w)
{
    fprintf(stderr, "R: ");
    encoding_print(vt, w->r);
    fprintf(stderr, "Z: ");
    encoding_print(vt, w->z);
}

static void
wire_init(const encoding_vtable *vt, const pp_vtable *pp_vt,
          wire *rop, const public_params *pp,
          bool init_r, bool init_z)
{
    if (init_r) {
        rop->r = encoding_new(vt, pp_vt, pp);
    }
    if (init_z) {
        rop->z = encoding_new(vt, pp_vt, pp);
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
wire_clear(const encoding_vtable *vt, wire *rop)
{
    if (rop->my_r) {
        encoding_free(vt, rop->r);
    }
    if (rop->my_z) {
        encoding_free(vt, rop->z);
    }
}

static void
wire_copy(const encoding_vtable *vt, const pp_vtable *pp_vt,
          wire *rop, const wire *source, const public_params *pp)
{
    rop->r = encoding_new(vt, pp_vt, pp);
    rop->z = encoding_new(vt, pp_vt, pp);
    encoding_set(vt, rop->r, source->r);
    encoding_set(vt, rop->z, source->z);
    rop->my_r = true;
    rop->my_z = true;
}

static int
wire_mul(const encoding_vtable *vt, const pp_vtable *pp_vt,
         wire *rop, const wire *x, const wire *y,
         const public_params *pp)
{
    if (encoding_mul(vt, pp_vt, rop->r, x->r, y->r, pp) == ERR)
        return ERR;
    if (encoding_mul(vt, pp_vt, rop->z, x->z, y->z, pp) == ERR)
        return ERR;
    return OK;
}

static int
wire_add(const encoding_vtable *vt, const pp_vtable *pp_vt,
         wire *rop, const wire *x, const wire *y,
         const public_params *pp)
{
    encoding *tmp = encoding_new(vt, pp_vt, pp);
    if (encoding_mul(vt, pp_vt, rop->r, x->r, y->r, pp) == ERR)
        return ERR;
    if (encoding_mul(vt, pp_vt, rop->z, x->z, y->r, pp) == ERR)
        return ERR;
    if (encoding_mul(vt, pp_vt, tmp,    y->z, x->r, pp) == ERR)
        return ERR;
    if (encoding_add(vt, pp_vt, rop->z, rop->z, tmp, pp) == ERR)
        return ERR;
    encoding_free(vt, tmp);
    return OK;
}

static int
wire_sub(const encoding_vtable *vt, const pp_vtable *pp_vt,
         wire *rop, const wire *x, const wire *y,
         const public_params *pp)
{
    encoding *tmp = encoding_new(vt, pp_vt, pp);
    if (encoding_mul(vt, pp_vt, rop->r, x->r, y->r, pp) == ERR)
        return ERR;
    if (encoding_mul(vt, pp_vt, rop->z, x->z, y->r, pp) == ERR)
        return ERR;
    if (encoding_mul(vt, pp_vt, tmp,    y->z, x->r, pp) == ERR)
        return ERR;
    if (encoding_sub(vt, pp_vt, rop->z, rop->z, tmp, pp) == ERR)
        return ERR;
    encoding_free(vt, tmp);
    return OK;
}

static int
_evaluate(int *rop, const int *inps, const obfuscation *obf, size_t nthreads)
{
    (void) nthreads;
    const obf_params_t *const op = obf->op;
    const public_params *const pp = obf->pp;
    acirc *const c = op->circ;
    int known[c->nrefs];
    wire *cache[c->nrefs];
    int ret = OK;

    memset(known, '\0', sizeof(known));
    memset(cache, '\0', sizeof(known));

    for (size_t o = 0; o < c->noutputs; o++) {
        acircref root = c->outrefs[o];
        acirc_topo_levels *topo = acirc_topological_levels(c, root);
        for (int lvl = 0; lvl < topo->nlevels; lvl++) {
            for (int i = 0; i < topo->level_sizes[lvl]; i++) {
                const acircref ref = topo->levels[lvl][i];
                if (known[ref])
                    continue;

                acirc_operation aop = c->gates[ref].op;
                const acircref *const args = c->gates[ref].args;
                wire *const w = my_calloc(1, sizeof(wire));
                if (aop == OP_INPUT) {
                    const size_t id = args[0];
                    const int b = inps[id];
                    wire_init_from_encodings(w, obf->R_ib[id][b], obf->Z_ib[id][b]);
                    if (LOG_DEBUG) {
                        fprintf(stderr, "INPUT\n");
                        wire_print(obf->enc_vt, w);
                        fprintf(stderr, "=====\n");
                    }
                } else if (aop == OP_CONST) {
                    const size_t id = args[0];
                    wire_init_from_encodings(w, obf->R_i[id], obf->Z_i[id]);
                    if (LOG_DEBUG) {
                        fprintf(stderr, "CONST\n");
                        wire_print(obf->enc_vt, w);
                        fprintf(stderr, "=====\n");
                    }
                } else {
                    wire *x = cache[args[0]];
                    wire *y = cache[args[1]];

                    wire_init(obf->enc_vt, obf->pp_vt, w, pp, true, true);

                    if (aop == OP_MUL) {
                        if (wire_mul(obf->enc_vt, obf->pp_vt, w, x, y, pp) == ERR)
                            ret = ERR;
                    } else if (aop == OP_ADD) {
                        if (wire_add(obf->enc_vt, obf->pp_vt, w, x, y, pp) == ERR)
                            ret = ERR;
                    } else if (aop == OP_SUB) {
                        if (wire_sub(obf->enc_vt, obf->pp_vt, w, x, y, pp) == ERR)
                            ret = ERR;
                    }
                    if (LOG_DEBUG) {
                        fprintf(stderr, "OP\n");
                        wire_print(obf->enc_vt, x);
                        wire_print(obf->enc_vt, y);
                        wire_print(obf->enc_vt, w);
                        fprintf(stderr, "==\n");
                    }
                }
                if (ret == ERR)
                    break;

                known[ref] = true;
                cache[ref] = w;
            }
            if (ret == ERR)
                break;
        }
        acirc_topo_levels_destroy(topo);

        if (ret == ERR)
            goto cleanup;

        wire tmp[1];
        wire tmp2[1];
        wire outwire[1];

        wire_copy(obf->enc_vt, obf->pp_vt, outwire, cache[root], pp);

        for (size_t k = 0; k < op->n; k++) {
            wire_init_from_encodings(tmp,
                                     obf->R_hat_ib_o[o][k][inps[k]],
                                     obf->Z_hat_ib_o[o][k][inps[k]]);
            if (LOG_DEBUG) {
                fprintf(stderr, "FINAL MULS\n");
                wire_print(obf->enc_vt, outwire);
                wire_print(obf->enc_vt, tmp);
            }
            wire_mul(obf->enc_vt, obf->pp_vt, outwire, outwire, tmp, pp);
            if (LOG_DEBUG) {   
                wire_print(obf->enc_vt, outwire);
                fprintf(stderr, "==========\n");
            }
        }

        wire_copy(obf->enc_vt, obf->pp_vt, tmp2, outwire, pp);
        wire_init_from_encodings(tmp, obf->R_o_i[o], obf->Z_o_i[o]);
        if (wire_sub(obf->enc_vt, obf->pp_vt, outwire, tmp2, tmp, pp) == ERR)
            ret = ERR;
        if (LOG_DEBUG) {
            fprintf(stderr, "FINAL SUB\n");
            wire_print(obf->enc_vt, tmp2);
            wire_print(obf->enc_vt, tmp);
            wire_print(obf->enc_vt, outwire);
            fprintf(stderr, "=========\n");
        }

        if (ret == OK) {
            rop[o] = encoding_is_zero(obf->enc_vt, obf->pp_vt, outwire->z, pp);
            if (rop[o] == ERR)
                ret = ERR;
        }

        wire_clear(obf->enc_vt, outwire);
        wire_clear(obf->enc_vt, tmp);
        wire_clear(obf->enc_vt, tmp2);
    }

cleanup:
    for (size_t x = 0; x < c->nrefs; x++) {
        if (known[x]) {
            wire_clear(obf->enc_vt, cache[x]);
            free(cache[x]);
        }
    }

    return ret;
}

obfuscator_vtable ab_obfuscator_vtable = {
    .new =  _obfuscation_new,
    .free = _obfuscation_free,
    .obfuscate = _obfuscate,
    .evaluate = _evaluate,
    .fwrite = _obfuscation_fwrite,
    .fread = _obfuscation_fread,
};
