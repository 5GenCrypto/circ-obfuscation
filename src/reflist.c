#include "reflist.h"
#include "util.h"

static void
push(ref_list_node *node, acircref ref)
{
    if (node->max == 0) {
        node->max = 2;
        node->refs = my_calloc(node->max, sizeof node->refs[0]);
    } else if (node->cur == node->max) {
        node->max *= 2;
        node->refs = realloc(node->refs, node->max * sizeof node->refs[0]);
    }
    node->refs[node->cur++] = ref;
}

ref_list *
ref_list_new(const acirc *c)
{
    const size_t nrefs = acirc_nrefs(c);
    ref_list *lst = my_calloc(1, sizeof lst[0]);
    lst->refs = my_calloc(nrefs, sizeof lst->refs[0]);
    for (size_t ref = 0; ref < nrefs; ++ref) {
        acirc_operation op = c->gates.gates[ref].op;
        if (op == OP_INPUT || op == OP_CONST)
            continue;
        acircref x = c->gates.gates[ref].args[0];
        acircref y = c->gates.gates[ref].args[1];
        push(&lst->refs[x], ref);
        push(&lst->refs[y], ref);
    }
    return lst;
}

void
ref_list_free(ref_list *lst)
{
    free(lst->refs);
    free(lst);
}
