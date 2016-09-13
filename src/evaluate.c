#include "evaluate.h"

#include "util.h"
#include <acirc.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
evaluate(const mmap_vtable *mmap, int *rop, const int *inps, obfuscation *obf,
         public_params *p)
{
    const obf_params *op = obf->op;
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

    for (size_t o = 0; o < op->circ->noutputs; o++) {
        acircref root = c->outrefs[o];
        acirc_topo_levels *topo = acirc_topological_levels((acirc *) c, root);
        for (int lvl = 0; lvl < topo->nlevels; lvl++) {
            for (int i = 0; i < topo->level_sizes[lvl]; i++) {
                acircref ref = topo->levels[lvl][i];
                if (known[ref])
                    continue;

                acirc_operation aop = c->ops[ref];
                acircref *args = c->args[ref];
                wire *w = lin_malloc(sizeof(wire));
                if (aop == XINPUT) {
                    size_t xid = args[0];
                    sym_id sym = op->chunker(xid, c->ninputs, op->n);
                    int k = sym.sym_number;
                    int s = input_syms[k];
                    wire_init_from_encodings(w, obf->R_ib[xid][s], obf->Z_ib[xid][s]);
                } else if (aop == YINPUT) {
                    size_t yid = args[0];
                    wire_init_from_encodings(w, obf->R_i[yid], obf->Z_i[yid]);
                } else {
                    wire *x = cache[args[0]];
                    wire *y = cache[args[1]];

                    wire_init(mmap, w, p, true, true);

                    if (aop == MUL) {
                        wire_mul(mmap, w, x, y, p);
                    } else if (aop == ADD) {
                        wire_add(mmap, w, x, y, p);
                    } else if (aop == SUB) {
                        wire_sub(mmap, w, x, y, p);
                    }
                }

                known[ref] = true;
                cache[ref] = w;
            }
        }
        acirc_topo_levels_destroy(topo);

        wire tmp[1];
        wire tmp2[1];
        wire outwire[1];

        wire_copy(mmap, outwire, cache[root], p);

        for (size_t k = 0; k < p->op->n; k++) {
            wire_init_from_encodings(tmp,
                                     obf->R_hat_ib[k][input_syms[k]],
                                     obf->Z_hat_ib[k][input_syms[k]]);
            wire_mul(mmap, outwire, outwire, tmp, p);
        }

        wire_copy(mmap, tmp2, outwire, p);
        wire_init_from_encodings(tmp, obf->R_o_i[0], obf->Z_o_i[0]);
        wire_sub(mmap, outwire, tmp2, tmp, p);

        rop[o] = encoding_is_zero(mmap, outwire->z, p);

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
