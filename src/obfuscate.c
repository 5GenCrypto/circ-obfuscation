#include "obfuscate.h"

// pows[0]   is the power of Y
// pows[i+1] is the power of X_i
void get_pows(int* pows, circuit* c) {
    pows[0] = ydegree(c, c->outref);
    for (int i = 0; i < c->ninputs; i++) {
        pows[i+1] = xdegree(c, c->outref, i);
    }
}

int num_indices(circuit* c) {
    int n = c->ninputs;
    return 1 +        // Y
           2*n +      // each X_i_b
           n +        // each Z_i
           n +        // each W_i
           n*(2*n-1); // the n straddling sets
}

void get_top_level_index(int* ix, circuit* c) {
    for (int i = 0; i < num_indices(c); i++) {
        ix[i] = 0;
    }
    ix[index_y(c)] = ydegree(c, c->outref);
    for (int i = 0; i < c->ninputs; i++) {
        assert(!ix[index_x(c, i, 0)]); // ensure there are no overlapping index assignments
        assert(!ix[index_x(c, i, 1)]);
        assert(!ix[index_z(c, i)]);
        assert(!ix[index_w(c, i)]);

        int xdeg = xdegree(c, c->outref, i);
        ix[index_x(c, i, 0)] = xdeg;
        ix[index_x(c, i, 1)] = xdeg;
        ix[index_z(c, i)] = 1;
        ix[index_w(c, i)] = 1;

        for (int j = 0; j < 2*c->ninputs-1; j++) {
            assert(!ix[index_f(c, i, j)]);
            ix[index_f(c, i, j)] = 1;
        }
    }
}

int index_y(circuit* c)               { return 0; }
int index_x(circuit* c, int i, int b) { return 1 + 2*i + b; }
int index_z(circuit* c, int i)        { return 1 + 2*c->ninputs + i; }
int index_w(circuit* c, int i)        { return 1 + 3*c->ninputs + i; }
int index_f(circuit* c, int i, int j) { return 1 + 4*c->ninputs + i*(2*c->ninputs-1) + j; }
