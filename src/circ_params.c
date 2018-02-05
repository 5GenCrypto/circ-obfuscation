#include "circ_params.h"
#include "util.h"

size_t
circ_params_ninputs(const acirc_t *circ)
{
    size_t ninputs = 0;
    for (size_t i = 0; i < acirc_nslots(circ); ++i)
        ninputs += acirc_symlen(circ, i);
    return ninputs;
}

size_t
circ_params_slot(const acirc_t *circ, size_t pos)
{
    size_t total = 0;
    for (size_t i = 0; i < acirc_nslots(circ); ++i) {
        if (pos >= total && pos < total + acirc_symlen(circ, i))
            return i;
        total += acirc_symlen(circ, i);
    }
    fprintf(stderr, "%s: %s: fatal: slot not found?\n",
            errorstr, __func__);
    abort();
}

size_t
circ_params_bit(const acirc_t *circ, size_t pos)
{
    size_t total = 0;
    for (size_t i = 0; i < acirc_nslots(circ); ++i) {
        if (pos >= total && pos < total + acirc_symlen(circ, i))
            return pos - total;
        total += acirc_symlen(circ, i);
    }
    fprintf(stderr, "%s: %s: fatal: bit not found?\n",
            errorstr, __func__);
    abort();
}

int
circ_params_print(const acirc_t *circ)
{
    const size_t has_consts = acirc_nconsts(circ) ? 1 : 0;
    fprintf(stderr, "Circuit parameters:\n");
    fprintf(stderr, "———— # symbols: .... %lu  [", acirc_nsymbols(circ));
    for (size_t i = 0; i < acirc_nsymbols(circ); ++i) {
        fprintf(stderr, " %lu ", acirc_symlen(circ, i));
    }
    fprintf(stderr, "]\n");
    fprintf(stderr, "———— # inputs: ..... %lu\n", acirc_ninputs(circ));
    fprintf(stderr, "———— # consts: ..... %lu\n", acirc_nconsts(circ));
    fprintf(stderr, "———— # outputs: .... %lu\n", acirc_noutputs(circ));
    fprintf(stderr, "———— # slots: ...... %lu\n", acirc_nslots(circ));
    for (size_t i = 0; i < acirc_nslots(circ); ++i) {
        size_t degree;
        if (i == acirc_nslots(circ) - has_consts)
            degree = acirc_max_const_degree(circ);
        else
            degree = acirc_max_var_degree(circ, i);
        if (degree == 0) return ERR; /* happens if we fail to parse the circuit */
        fprintf(stderr, "————   slot %lu: ..... %lu (%lu) [%lu]\n", i,
                acirc_symlen(circ, i), acirc_symnum(circ, i), degree);
    }
    fprintf(stderr, "———— # refs: ....... %lu\n", acirc_nrefs(circ));
    fprintf(stderr, "———— # gates: ...... %lu\n", acirc_ngates(circ));
    fprintf(stderr, "———— # muls: ....... %lu\n", acirc_nmuls(circ));
    fprintf(stderr, "———— depth: ........ %lu\n", acirc_max_depth(circ));
    fprintf(stderr, "———— saved? ........ %s", acirc_is_saved(circ) ? "✓" : "✗");
    if (acirc_is_saved(circ))
        fprintf(stderr, " [%s.encodings/]", acirc_fname(circ));
    fprintf(stderr, "\n");
    fprintf(stderr, "———— binary? ....... %s\n", acirc_is_binary(circ) ? "✓" : "✗");
    return OK;
}

size_t *
get_input_syms(const long *inputs, size_t ninputs, const acirc_t *circ)
{
    const size_t nsymbols = acirc_nsymbols(circ);
    size_t *symbols;
    size_t k = 0;

    if ((symbols = my_calloc(nsymbols, sizeof symbols[0])) == NULL)
        return NULL;
    for (size_t i = 0; i < nsymbols; i++) {
        symbols[i] = 0;
        for (size_t j = 0; j < acirc_symlen(circ, i); j++) {
            if (k >= ninputs) {
                fprintf(stderr, "%s: too many symbols for input length\n",
                        errorstr);
                goto error;
            }
            if (acirc_is_sigma(circ, i))
                symbols[i] += inputs[k++] * j;
            else if (j >= sizeof(size_t) * 8){
                fprintf(stderr, "%s: overflow detected: symbol length > 64\n",
                        errorstr);
                goto error;
            } else {
                symbols[i] += inputs[k++] << j;
            }
        }
        if (symbols[i] >= acirc_symnum(circ, i)) {
            fprintf(stderr, "%s: invalid input for symbol %lu (%ld > %lu)\n",
                    errorstr, i, symbols[i], acirc_symnum(circ, i));
            goto error;
        }
    }
    return symbols;
error:
    if (symbols)
        free(symbols);
    return NULL;
}
