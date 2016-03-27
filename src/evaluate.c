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
                        encoding_mul(w.r, x.r, y.r);
                        encoding_mul(w.z, x.z, y.z);
                        w.d = x.d + y.d;
                    }

                    else if (op == ADD || (op == SUB && x.d <= y.d)) {
                        if (x.d > y.d) {
                            assert(op == ADD);
                            x = cache[args[1]];
                            y = cache[args[0]];
                        }

                        size_t delta = y.d - x.d;
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

                        encoding_mul(w.z, x.z, y.r);
                        if (delta > 0)
                            encoding_mul(w.z, w.z, &zstar);
                        encoding_mul(&tmp, y.z, x.r);

                        if (op == ADD)
                            encoding_add(w.z, w.z, &tmp);
                        else
                            encoding_sub(w.z, w.z, &tmp);

                        encoding_mul(w.r, x.r, y.r);
                        w.d = y.d;

                        if (delta > 1)
                            encoding_clear(&zstar);

                        encoding_clear(&tmp);
                    }

                    // in the case that the degree of x is greather than the degree of y,
                    // we can't just permute the operands like with addition, since
                    // subtraction isn't associative
                    else if (op == SUB) {
                        assert (x.d > y.d);

                        encoding zstar;
                        size_t delta = x.d - y.d;
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

                        encoding_mul(w.z, x.z, y.r);
                        encoding_mul(&tmp, y.z, x.r);
                        if (delta > 0)
                            encoding_mul(&tmp, &tmp, &zstar);
                        encoding_sub(w.z, w.z, &tmp);

                        encoding_mul(w.r, x.r, y.r);
                        w.d = x.d;

                        if (delta > 1)
                            encoding_clear(&zstar);

                        encoding_clear(&tmp);
                    }
                }

                known[ref] = true;
                cache[ref] = w;
            }
        }

        topo_levels_destroy(topo);

    }

}
