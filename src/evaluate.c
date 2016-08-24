#include "evaluate.h"

#include "util.h"
#include <acirc.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// TODO: save zstar pows for reuse within the circuit

void evaluate (int *rop, const int *inps, obfuscation *obf, public_params *p, bool is_rachel)
{
    acirc *c = obf->op->circ;

    // determine each assignment s \in \Sigma from the input bits
    int *input_syms = lin_malloc(obf->op->c * sizeof(int));
    for (int i = 0; i < obf->op->c; i++) {
        input_syms[i] = 0;
        for (int j = 0; j < obf->op->ell; j++) {
            sym_id sym = { i, j };
            acircref inp_bit = obf->op->rchunker(sym, c->ninputs, obf->op->c);
            if (is_rachel)
                input_syms[i] += inps[inp_bit] * j;
            else
                input_syms[i] += inps[inp_bit] << j;
        }
    }

    int   *known = lin_calloc(c->nrefs, sizeof(acircref));
    wire **cache = lin_malloc(c->nrefs * sizeof(wire*));

    for (int o = 0; o < obf->op->circ->noutputs; o++) {
        acircref root = c->outrefs[o];

        acirc_topo_levels *topo = acirc_topological_levels(c, root);

        for (int lvl = 0; lvl < topo->nlevels; lvl++) {

            // #pragma omp parallel for
            for (int i = 0; i < topo->level_sizes[lvl]; i++) {

                acircref ref = topo->levels[lvl][i];
                if (known[ref]) continue;

                acirc_operation op = c->ops[ref];
                acircref *args = c->args[ref];

                wire *w = lin_malloc(sizeof(wire));

                if (op == XINPUT) {
                    size_t xid = args[0];
                    sym_id sym = obf->op->chunker(xid, c->ninputs, obf->op->c);
                    int k = sym.sym_number;
                    int j = sym.bit_number;
                    int s = input_syms[k];
                    wire_init_from_encodings(w, p, obf->Rks[k][s], obf->Zksj[k][s][j]);
                }

                else if (op == YINPUT) {
                    size_t yid = args[0];
                    wire_init_from_encodings(w, p, obf->Rc, obf->Zcj[yid]);
                }

                else { // op is some kind of gate
                    wire *x = cache[args[0]];
                    wire *y = cache[args[1]];

                    if (op == MUL) {
                        wire_init(w, p, 1, 1);
                        wire_mul(w, x, y, p);
                    }

                    /*else if (wire_type_eq(x, y) && encoding_eq(x->r, y->r)) {*/
                    else if (wire_type_eq(x, y)) {
                        wire_init(w, p, 0, 1);
                        if (op == ADD) {
                            wire_constrained_add(w, x, y, obf, p);
                        } else if (op == SUB) {
                            wire_constrained_sub(w, x, y, obf, p);
                        }
                    }

                    else {
                        wire_init(w, p, 1, 1);
                        if (op == ADD) {
                            wire_add(w, x, y, obf, p);
                        } else if (op == SUB) {
                            wire_sub(w, x, y, obf, p);
                        }
                    }
                }

                known[ref] = true;
                cache[ref] = w;
            }
        }
        acirc_topo_levels_destroy(topo);

        wire tmp[1]; // stack allocated pointers
        wire outwire[1];
        wire_copy(outwire, cache[root], p);

        // input consistency
        for (int k = 0; k < p->op->c; k++) {
            wire_init_from_encodings(tmp, p,
                obf->Rhatkso[k][input_syms[k]][o],
                obf->Zhatkso[k][input_syms[k]][o]
            );
            wire_mul(outwire, outwire, tmp, p);
        }

        // output consistency
        wire_init_from_encodings(tmp, p, obf->Rhato[o], obf->Zhato[o]);
        wire_mul(outwire, outwire, tmp, p);

        // authentication
        wire_init_from_encodings(tmp, p, obf->Rbaro[o], obf->Zbaro[o]);
        wire_sub(outwire, outwire, tmp, obf, p);
        wire_clear(tmp);

        rop[o] = encoding_is_zero(outwire->z, p);

        wire_clear(outwire);
    }

    for (acircref x = 0; x < c->nrefs; x++) {
        if (known[x]) {
            wire_clear(cache[x]);
            free(cache[x]);
        }
    }
    free(cache);
    free(known);
    free(input_syms);
}

void wire_init (wire *rop, public_params *p, int init_r, int init_z)
{
    if (init_r) {
        rop->r = lin_malloc(sizeof(encoding));
        encoding_init(rop->r, p->op);
    }
    if (init_z) {
        rop->z = lin_malloc(sizeof(encoding));
        encoding_init(rop->z, p->op);
    }
    rop->d = 0;
    rop->my_r = init_r;
    rop->my_z = init_z;
}

void wire_init_from_encodings (wire *rop, public_params *p, encoding *r, encoding *z)
{
    rop->r = r;
    rop->z = z;
    rop->my_r = 0;
    rop->my_z = 0;
    rop->d = z->lvl->mat[p->op->q][p->op->c+1];
}

void wire_clear (wire *rop)
{
    if (rop->my_r) {
        encoding_clear(rop->r);
        free(rop->r);
    }
    if (rop->my_z) {
        encoding_clear(rop->z);
        free(rop->z);
    }
}

void wire_copy (wire *rop, wire *source, public_params *p)
{
    rop->r = lin_malloc(sizeof(encoding));
    encoding_init(rop->r, p->op);
    encoding_set(rop->r, source->r);
    rop->my_r = 1;

    rop->z = lin_malloc(sizeof(encoding));
    encoding_init(rop->z, p->op);
    encoding_set(rop->z, source->z);
    rop->my_z = 1;

    rop->d = source->d;
}

void wire_mul (wire *rop, wire *x, wire *y, public_params *p)
{
    encoding_mul(rop->r, x->r, y->r, p);
    encoding_mul(rop->z, x->z, y->z, p);
    rop->d = x->d + y->d;
}

void wire_add (wire *rop, wire *x, wire *y, obfuscation *obf, public_params *p)
{
    if (x->d > y->d) {
        wire_add(rop, y, x, obf, p);
        return;
    }

    size_t d = y->d - x->d;
    encoding zstar;
    if (d > 1) {
        encoding_init(&zstar, p->op);
        encoding_mul(&zstar, obf->Zstar, obf->Zstar, p);
        for (int j = 2; j < d; j++)
            encoding_mul(&zstar, &zstar, obf->Zstar, p);
    } else {
        zstar = *obf->Zstar;
    }

    encoding tmp;
    encoding_init(&tmp, p->op);

    encoding_mul(rop->z, x->z, y->r, p);
    if (d > 0)
        encoding_mul(rop->z, rop->z, &zstar, p);
    encoding_mul(&tmp, y->z, x->r, p);
    encoding_add(rop->z, rop->z, &tmp, p);

    encoding_mul(rop->r, x->r, y->r, p);

    rop->d = y->d;

    if (d > 1)
        encoding_clear(&zstar);
    encoding_clear(&tmp);
}

void wire_sub(wire *rop, wire *x, wire *y, obfuscation *obf, public_params *p)
{
    size_t d = abs(y->d - x->d);
    encoding zstar;
    if (d > 1) {
        encoding_init(&zstar, p->op);
        encoding_mul(&zstar, obf->Zstar, obf->Zstar, p);
        for (int j = 2; j < d; j++)
            encoding_mul(&zstar, &zstar, obf->Zstar, p);
    } else {
        zstar = *obf->Zstar;
    }

    encoding tmp;
    encoding_init(&tmp, p->op);

    if (x->d <= y->d) {
        encoding_mul(rop->z, x->z, y->r, p);
        if (d > 0)
            encoding_mul(rop->z, rop->z, &zstar, p);
        encoding_mul(&tmp, y->z, x->r, p);
        encoding_sub(rop->z, rop->z, &tmp, p);
        rop->d = y->d;
    } else {
        encoding_mul(rop->z, x->z, y->r, p);
        encoding_mul(&tmp, y->z, x->r, p);
        if (d > 0)
            encoding_mul(&tmp, &tmp, &zstar, p);
        encoding_sub(rop->z, rop->z, &tmp, p);
        rop->d = x->d;
    }
    encoding_mul(rop->r, x->r, y->r, p);

    if (d > 1)
        encoding_clear(&zstar);

    encoding_clear(&tmp);
}

void wire_constrained_add (wire *rop, wire *x, wire *y, obfuscation *obf, public_params *p)
{
    if (x->d > y->d) {
        wire_constrained_add(rop, y, x, obf, p);
        return;
    }

    size_t d = abs(y->d - x->d);
    encoding zstar;
    if (d > 1) {
        encoding_init(&zstar, p->op);
        encoding_mul(&zstar, obf->Zstar, obf->Zstar, p);
        for (int j = 2; j < d; j++)
            encoding_mul(&zstar, &zstar, obf->Zstar, p);
    } else {
        zstar = *obf->Zstar;
    }

    if (d > 0) {
        printf("multiplying!\n");
        encoding_mul(rop->z, x->z, &zstar, p);
        encoding_add(rop->z, rop->z, y->z, p);
    } else {
        encoding_add(rop->z, x->z, y->z, p);
    }
    rop->d = y->d;
    rop->r = x->r;

    if (d > 1)
        encoding_clear(&zstar);
}

void wire_constrained_sub(wire *rop, wire *x, wire *y, obfuscation *obf, public_params *p)
{
    size_t d = abs(y->d - x->d);
    encoding zstar;
    if (d > 1) {
        encoding_init(&zstar, p->op);
        encoding_mul(&zstar, obf->Zstar, obf->Zstar, p);
        for (int j = 2; j < d; j++)
            encoding_mul(&zstar, &zstar, obf->Zstar, p);
    } else {
        zstar = *obf->Zstar;
    }

    if (x->d <= y->d) {
        if (d > 0) {
            encoding_mul(rop->z, x->z, &zstar, p);
            encoding_sub(rop->z, rop->z, y->z, p);
        } else {
            encoding_sub(rop->z, x->z, y->z, p);
        }
        rop->d = y->d;
    } else {
        // x->d > y->d && d > 0
        encoding tmp;
        encoding_init(&tmp, p->op);
        encoding_mul(&tmp, y->z, &zstar, p);
        encoding_sub(rop->z, rop->z, &tmp, p);
        encoding_clear(&tmp);
        rop->d = x->d;
    }
    rop->r = x->r;

    if (d > 1)
        encoding_clear(&zstar);
}

int wire_type_eq (wire *x, wire *y)
{
    if (!level_eq(x->r->lvl, y->r->lvl))
        return 0;
    if (!level_eq_z(x->z->lvl, y->z->lvl))
        return 0;
    return 1;
}
