#include "evaluate.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

void evaluate (int *rop, const int *inps, obfuscation *obf, fake_params *p)
{
    circuit *c = obf->op->circ;
    int  *known = calloc(c->nrefs, sizeof(circref));
    wire *cache = malloc(c->nrefs * sizeof(wire));

    // determine each assignment s \in \Sigma from the input bits
    int *input_syms = malloc(obf->op->c * sizeof(int));
    for (int i = 0; i < obf->op->c; i++) {
        input_syms[i] = 0;
        for (int j = 0; j < obf->op->ell; j++) {
            sym_id sym = { i, j };
            circref inp_bit = obf->op->rchunker(sym, c->ninputs, obf->op->c);
            input_syms[i] += inps[inp_bit] << j;
        }
    }

    for (int o = 0; o < obf->op->circ->noutputs; o++) {
        circref root = c->outrefs[o];

        topo_levels *topo = topological_levels(c, root);

        for (int lvl = 0; lvl < topo->nlevels; lvl++) {
            // TODO: parallelize here
            for (int i = 0; i < topo->level_sizes[lvl]; i++) {

                circref ref = topo->levels[lvl][i];
                if (known[ref]) continue;

                operation op  = c->ops[ref];
                circref *args = c->args[ref];

                wire w;

                if (op == YINPUT) {
                    size_t yid = args[0];
                    w.r = obf->Rc;
                    w.z = obf->Zcj[yid];
                    w.d = 1;
                }

                else if (op == XINPUT) {
                    size_t xid = args[0];
                    sym_id sym = obf->op->chunker(xid, c->ninputs, obf->op->c);
                    int k = sym.sym_number;
                    int j = sym.bit_number;
                    int s = input_syms[k];
                    w.r = obf->Rks[k][s];
                    w.z = obf->Zksj[k][s][j];
                    w.d = 1;
                }

                else { // op is some kind of gate
                    w.r = malloc(sizeof(encoding));
                    w.z = malloc(sizeof(encoding));
                    encoding_init(w.r, p);
                    encoding_init(w.z, p);

                    wire x = cache[args[0]];
                    wire y = cache[args[1]];

                    if (op == MUL) {
                        wire_mul(&w, &x, &y);
                    } else if (op == ADD) {
                        wire_add(&w, &x, &y, obf, p);
                    } else if (op == SUB) {
                        wire_sub(&w, &x, &y, obf, p);
                    }
                }

                known[ref] = true;
                cache[ref] = w;
            }
        }

        topo_levels_destroy(topo);

    }

}

void wire_mul (wire *rop, wire *x, wire *y)
{
    encoding_mul(rop->r, x->r, y->r);
    encoding_mul(rop->z, x->z, y->z);
    rop->d = x->d + y->d;
}

void wire_add (wire *rop, wire *x, wire *y, obfuscation *obf, fake_params *p)
{
    if (x->d > y->d) {
        wire_add(rop, y, x, obf, p);
        return;
    }

    size_t delta = y->d - x->d;
    encoding zstar;
    if (delta > 1) {
        encoding_init(&zstar, p);
        encoding_mul(&zstar, obf->Zstar, obf->Zstar);
        for (int j = 2; j < delta; j++)
            encoding_mul(&zstar, &zstar, obf->Zstar);
    } else {
        zstar = *obf->Zstar;
    }

    encoding tmp;
    encoding_init(&tmp, p);

    encoding_mul(rop->z, x->z, y->r);
    if (delta > 0)
        encoding_mul(rop->z, rop->z, &zstar);
    encoding_mul(&tmp, y->z, x->r);

    encoding_add(rop->z, rop->z, &tmp);

    encoding_mul(rop->r, x->r, y->r);
    rop->d = y->d;

    if (delta > 1)
        encoding_clear(&zstar);

    encoding_clear(&tmp);
}

void wire_sub(wire *rop, wire *x, wire *y, obfuscation *obf, fake_params *p)
{
    size_t delta = y->d - x->d;
    encoding zstar;
    if (delta > 1) {
        encoding_init(&zstar, p);
        encoding_mul(&zstar, obf->Zstar, obf->Zstar);
        for (int j = 2; j < delta; j++)
            encoding_mul(&zstar, &zstar, obf->Zstar);
    } else {
        zstar = *obf->Zstar;
    }

    encoding tmp;
    encoding_init(&tmp, p);

    if (x->d <= y->d) {
        encoding_mul(rop->z, x->z, y->r);
        if (delta > 0)
            encoding_mul(rop->z, rop->z, &zstar);
        encoding_mul(&tmp, y->z, x->r);
        encoding_add(rop->z, rop->z, &tmp);
        encoding_mul(rop->r, x->r, y->r);
        rop->d = y->d;
    } else {
        encoding_mul(rop->z, x->z, y->r);
        encoding_mul(&tmp, y->z, x->r);
        if (delta > 0)
            encoding_mul(&tmp, &tmp, &zstar);
        encoding_sub(rop->z, rop->z, &tmp);

        encoding_mul(rop->r, x->r, y->r);
        rop->d = x->d;
    }

    if (delta > 1)
        encoding_clear(&zstar);

    encoding_clear(&tmp);
}
