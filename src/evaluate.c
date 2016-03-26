#include "evaluate.h"

#include <stdlib.h>

void evaluate (bool *rop, const bool *inps, obfuscation *obf, fake_params *p)
{
    circuit *c = obf->op->circ;
    bool *known = malloc(c->nrefs * sizeof(circref));
    wire *cache = malloc(c->nrefs * sizeof(wire));

    for (int o = 0; o < obf->op->circ->noutputs; o++) {
        circref root = c->outrefs[o];

        int **topo_levels = malloc(c->nrefs * sizeof(circref));
        int *level_sizes  = malloc(c->nrefs * sizeof(int));
        int nlevels = topological_levels (topo_levels, level_sizes, c, root);

        for (int level = 0; level < nlevels; level++) {
            for (int i = 0; i < level_sizes[level]; i++) {
                circref ref   = topo_levels[level][i];
                operation op  = c->ops[ref];
                circref *args = c->args[ref];

                wire *w = malloc(sizeof(wire));

                if (op == YINPUT) {
                    size_t yid = args[0];
                    w->r = obf->Rc;
                    w->z = obf->Zcj[yid];
                }

                // TODO: figure out which s \in \Sigma we have
                else if (op == XINPUT) {
                    size_t xid = args[0];
                    sym_id sym = obf->op->input_chunker(xid, obf->op->ninputs, obf->op->c);
                    /*w->r = obf->*/
                }

                cache[ref] = w;
            }
        }

    }

}
