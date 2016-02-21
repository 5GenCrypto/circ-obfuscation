#include "circuit.h"

void circ_init( circuit *c ) {

}

void circ_add_test(circuit *c, bitvector *inp, bool out) {
    testcase t = { inp, out };
    list_add_node(c->tests, &t);
}
