#include "extra.h"
#include "obf_params.h"
#include "../util.h"

size_t
polylog_nlevels(obf_params_t *op)
{
    return op->nlevels;
}

size_t
polylog_nmuls(obf_params_t *op)
{
    return op->nmuls;
}

mmap_polylog_switch_params *
polylog_switch_params(obf_params_t *op, size_t nzs)
{
    mmap_polylog_switch_params *sparams;
    sparams = calloc(polylog_nlevels(op), sizeof sparams[0]);
    for (size_t i = 0; i < polylog_nlevels(op); ++i) {
        sparams[i].source = 0;
        sparams[i].target = 1;
        sparams[i].ix = my_calloc(nzs, sizeof sparams[i].ix[0]);
    }
    return sparams;
}
