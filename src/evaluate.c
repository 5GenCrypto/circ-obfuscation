#include "evaluate.h"

#include "util.h"
#include <acirc.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// TODO: save zstar pows for reuse within the circuit

void
evaluate(const mmap_vtable *mmap, int *rop, const int *inps, obfuscation *obf,
         public_params *p, bool is_rachel)
{
    acirc *c = obf->op->circ;

    // determine each assignment s \in \Sigma from the input bits
    int input_syms[obf->op->c];
    for (size_t i = 0; i < obf->op->c; i++) {
        input_syms[i] = 0;
        for (size_t j = 0; j < obf->op->ell; j++) {
            sym_id sym = { i, j };
            acircref inp_bit = obf->op->rchunker(sym, c->ninputs, obf->op->c);
            if (is_rachel)
                input_syms[i] += inps[inp_bit] * j;
            else
                input_syms[i] += inps[inp_bit] << j;
        }
    }

    int *known = lin_calloc(c->nrefs, sizeof(int));
    wire **cache = lin_malloc(c->nrefs * sizeof(wire*));

    for (size_t o = 0; o < obf->op->circ->noutputs; o++) {
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
                } else if (op == YINPUT) {
                    size_t yid = args[0];
                    wire_init_from_encodings(w, p, obf->Rc, obf->Zcj[yid]);
                } else { // op is some kind of gate
                    wire *x = cache[args[0]];
                    wire *y = cache[args[1]];

                    if (op == MUL) {
                        wire_init(mmap, w, p, 1, 1);
                        wire_mul(mmap, w, x, y, p);
                    } else if (wire_type_eq(x, y)) {
                        wire_init(mmap, w, p, 0, 1);
                        if (op == ADD) {
                            wire_constrained_add(mmap, w, x, y, obf, p);
                        } else if (op == SUB) {
                            wire_constrained_sub(mmap, w, x, y, obf, p);
                        }
                    } else {
                        wire_init(mmap, w, p, 1, 1);
                        if (op == ADD) {
                            wire_add(mmap, w, x, y, obf, p);
                        } else if (op == SUB) {
                            wire_sub(mmap, w, x, y, obf, p);
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
        wire_copy(mmap, outwire, cache[root], p);

        // input consistency
        for (size_t k = 0; k < p->op->c; k++) {
            wire_init_from_encodings(tmp, p,
                obf->Rhatkso[k][input_syms[k]][o],
                obf->Zhatkso[k][input_syms[k]][o]
            );
            wire_mul(mmap, outwire, outwire, tmp, p);
        }

        // output consistency
        wire_init_from_encodings(tmp, p, obf->Rhato[o], obf->Zhato[o]);
        wire_mul(mmap, outwire, outwire, tmp, p);

        // authentication
        wire_init_from_encodings(tmp, p, obf->Rbaro[o], obf->Zbaro[o]);
        wire_sub(mmap, outwire, outwire, tmp, obf, p);
        wire_clear(mmap, tmp);

        rop[o] = encoding_is_zero(mmap, outwire->z, p);

        wire_clear(mmap, outwire);
    }

    for (acircref x = 0; x < c->nrefs; x++) {
        if (known[x]) {
            wire_clear(mmap, cache[x]);
            free(cache[x]);
        }
    }
    free(cache);
    free(known);
}

void
wire_init(const mmap_vtable *mmap, wire *rop, public_params *p, int init_r,
          int init_z)
{
    if (init_r) {
        rop->r = encoding_new(mmap, p, p->op);
    }
    if (init_z) {
        rop->z = encoding_new(mmap, p, p->op);
    }
    rop->d = 0;
    rop->my_r = init_r;
    rop->my_z = init_z;
}

void
wire_init_from_encodings(wire *rop, public_params *p, encoding *r, encoding *z)
{
    rop->r = r;
    rop->z = z;
    rop->my_r = 0;
    rop->my_z = 0;
    rop->d = z->lvl->mat[p->op->q][p->op->c+1];
}

void
wire_clear(const mmap_vtable *mmap, wire *rop)
{
    if (rop->my_r) {
        encoding_free(mmap, rop->r);
    }
    if (rop->my_z) {
        encoding_free(mmap, rop->z);
    }
}

void
wire_copy(const mmap_vtable *mmap, wire *rop, wire *source, public_params *p)
{
    rop->r = encoding_new(mmap, p, p->op);
    encoding_set(mmap, rop->r, source->r);
    rop->my_r = 1;

    rop->z = encoding_new(mmap, p, p->op);
    encoding_set(mmap, rop->z, source->z);
    rop->my_z = 1;

    rop->d = source->d;
}

void
wire_mul(const mmap_vtable *mmap, wire *rop, wire *x, wire *y, public_params *p)
{
    encoding_mul(mmap, rop->r, x->r, y->r, p);
    encoding_mul(mmap, rop->z, x->z, y->z, p);
    rop->d = x->d + y->d;
}

void
wire_add(const mmap_vtable *mmap, wire *rop, wire *x, wire *y, obfuscation *obf,
         public_params *p)
{
    if (x->d > y->d) {
        wire_add(mmap, rop, y, x, obf, p);
    } else {
        encoding *zstar, *tmp;
        size_t d = y->d - x->d;

        encoding_mul(mmap, rop->r, x->r, y->r, p);

        if (d > 1) {
            /* Compute Zstar^d */
            zstar = encoding_new(mmap, p, p->op);
            encoding_mul(mmap, zstar, obf->Zstar, obf->Zstar, p);
            for (size_t j = 2; j < d; j++)
                encoding_mul(mmap, zstar, zstar, obf->Zstar, p);
        } else {
            zstar = obf->Zstar;
        }

        tmp = encoding_new(mmap, p, p->op);
        encoding_mul(mmap, rop->z, x->z, y->r, p);
        if (d > 0)
            encoding_mul(mmap, rop->z, rop->z, zstar, p);
        encoding_mul(mmap, tmp, y->z, x->r, p);
        encoding_add(mmap, rop->z, rop->z, tmp, p);

        rop->d = y->d;

        if (d > 1) {
            encoding_free(mmap, zstar);
        }
        encoding_free(mmap, tmp);
    }
}

void
wire_sub(const mmap_vtable *mmap, wire *rop, wire *x, wire *y, obfuscation *obf,
         public_params *p)
{
    size_t d = abs((int) y->d - (int) x->d);
    encoding *zstar, *tmp;

    if (d > 1) {
        zstar = encoding_new(mmap, p, p->op);
        encoding_mul(mmap, zstar, obf->Zstar, obf->Zstar, p);
        for (size_t j = 2; j < d; j++)
            encoding_mul(mmap, zstar, zstar, obf->Zstar, p);
    } else {
        zstar = obf->Zstar;
    }

    tmp = encoding_new(mmap, p, p->op);

    if (x->d <= y->d) {
        encoding_mul(mmap, rop->z, x->z, y->r, p);
        if (d > 0)
            encoding_mul(mmap, rop->z, rop->z, zstar, p);
        encoding_mul(mmap, tmp, y->z, x->r, p);
        encoding_sub(mmap, rop->z, rop->z, tmp, p);
        rop->d = y->d;
    } else {
        encoding_mul(mmap, rop->z, x->z, y->r, p);
        encoding_mul(mmap, tmp, y->z, x->r, p);
        if (d > 0)
            encoding_mul(mmap, tmp, tmp, zstar, p);
        encoding_sub(mmap, rop->z, rop->z, tmp, p);
        rop->d = x->d;
    }
    encoding_mul(mmap, rop->r, x->r, y->r, p);

    if (d > 1)
        encoding_free(mmap, zstar);
    encoding_free(mmap, tmp);
}

void
wire_constrained_add(const mmap_vtable *mmap, wire *rop, wire *x, wire *y,
                     obfuscation *obf, public_params *p)
{
    if (x->d > y->d) {
        wire_constrained_add(mmap, rop, y, x, obf, p);
    } else {
        size_t d = y->d - x->d;
        encoding *zstar;

        if (d > 1) {
            zstar = encoding_new(mmap, p, p->op);
            encoding_mul(mmap, zstar, obf->Zstar, obf->Zstar, p);
            for (size_t j = 2; j < d; j++)
                encoding_mul(mmap, zstar, zstar, obf->Zstar, p);
        } else {
            zstar = obf->Zstar;
        }

        if (d > 0) {
            encoding_mul(mmap, rop->z, x->z, zstar, p);
            encoding_add(mmap, rop->z, rop->z, y->z, p);
        } else {
            encoding_add(mmap, rop->z, x->z, y->z, p);
        }
        rop->r = x->r;
        rop->d = y->d;

        if (d > 1)
            encoding_free(mmap, zstar);
    }
}

void
wire_constrained_sub(const mmap_vtable *mmap, wire *rop, wire *x, wire *y,
                     obfuscation *obf, public_params *p)
{
    size_t d = abs((int) y->d - (int) x->d);
    encoding *zstar;
    if (d > 1) {
        zstar = encoding_new(mmap, p, p->op);
        encoding_mul(mmap, zstar, obf->Zstar, obf->Zstar, p);
        for (size_t j = 2; j < d; j++)
            encoding_mul(mmap, zstar, zstar, obf->Zstar, p);
    } else {
        zstar = obf->Zstar;
    }

    if (x->d <= y->d) {
        if (d > 0) {
            encoding_mul(mmap, rop->z, x->z, zstar, p);
            encoding_sub(mmap, rop->z, rop->z, y->z, p);
        } else {
            encoding_sub(mmap, rop->z, x->z, y->z, p);
        }
        rop->d = y->d;
    } else {
        // x->d > y->d && d > 0
        encoding *tmp;
        tmp = encoding_new(mmap, p, p->op);
        encoding_mul(mmap, tmp, y->z, zstar, p);
        encoding_sub(mmap, rop->z, rop->z, tmp, p);
        encoding_free(mmap, tmp);
        rop->d = x->d;
    }
    rop->r = x->r;

    if (d > 1)
        encoding_free(mmap, zstar);
}

int
wire_type_eq(wire *x, wire *y)
{
    if (!level_eq(x->r->lvl, y->r->lvl))
        return 0;
    if (!level_eq_z(x->z->lvl, y->z->lvl))
        return 0;
    return 1;
}
