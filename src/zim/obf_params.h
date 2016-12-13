#ifndef __ZIM__OBF_PARAMS_H__
#define __ZIM__OBF_PARAMS_H__

#include <acirc.h>
#include <stddef.h>

struct obf_params_t {
    size_t ninputs;
    size_t nconsts;
    size_t noutputs;
    size_t npowers;
    const acirc *circ;
};

#endif
