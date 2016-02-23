#ifndef __OBFUSCATE__
#define __OBFUSCATE__

#include <stdlib.h>
#include "circuit.h"

int num_indices(circuit* c);

void get_pows(int* pows, circuit* c);
void get_top_level_index(int* ix, circuit* c);

int index_y(circuit* c);
int index_x(circuit* c, int i, int b);
int index_z(circuit* c, int i);
int index_w(circuit* c, int i);
int index_f(circuit* c, int i, int j);

#endif
