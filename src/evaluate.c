#include "evaluate.h"

#include <stdlib.h>

void evaluate (bool *rop, const bool *inps, obfuscation *obf, fake_params *p)
{
    circuit *c = obf->op->circ;
    bool *known = malloc(c->nrefs * sizeof(circref));
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

        int **topo_levels = malloc(c->nrefs * sizeof(circref));
        int *level_sizes  = malloc(c->nrefs * sizeof(int));
        int nlevels = topological_levels (topo_levels, level_sizes, c, root);

        for (int level = 0; level < nlevels; level++) {
            // TODO: parallelize here
            for (int i = 0; i < level_sizes[level]; i++) {
                circref ref   = topo_levels[level][i];
                if (known[ref]) continue;

                operation op  = c->ops[ref];
                circref *args = c->args[ref];

                wire w;

                if (op == YINPUT) {
                    size_t yid = args[0];
                    w.r = obf->Rc;
                    w.z = obf->Zcj[yid];
                }

                else if (op == XINPUT) {
                    size_t xid = args[0];
                    sym_id sym = obf->op->chunker(xid, c->ninputs, obf->op->c);
                    int k = sym.sym_number;
                    int j = sym.bit_number;
                    int s = input_syms[k];
                    w.r = obf->Rks[k][s];
                    w.z = obf->Zksj[k][s][j];
                }

                else if (op == MUL) {

                    if (x->d > y->d) {
                        return encoding_add(rop, y, x);
                    }

                    size_t delta = y->d - x->d;

                }

                known[ref] = true;
                cache[ref] = w;
            }
        }

    }

}
