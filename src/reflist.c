#include "reflist.h"
#include "util.h"

static ref_list_node *
ref_list_node_create(acircref ref)
{
    ref_list_node *new = my_calloc(1, sizeof(ref_list_node));
    new->next = NULL;
    new->ref  = ref;
    return new;
}

static ref_list *
ref_list_create(void)
{
    ref_list *list = my_calloc(1, sizeof(ref_list));
    list->first = NULL;
    return list;
}

static void
ref_list_destroy(ref_list *list)
{
    ref_list_node *cur = list->first;
    while (cur != NULL) {
        ref_list_node *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    free(list);
}

static void
ref_list_push(ref_list *list, acircref ref)
{
    ref_list_node *cur = list->first;
    if (cur == NULL) {
        list->first = ref_list_node_create(ref);
        return;
    }
    while (1) {
        if (cur->next == NULL) {
            cur->next = ref_list_node_create(ref);
            return;
        }
        cur = cur->next;
    }
}

ref_list **
ref_lists_new(const acirc *c)
{
    ref_list **lists = my_calloc(acirc_nrefs(c), sizeof lists[0]);
    for (size_t i = 0; i < acirc_nrefs(c); i++) {
        lists[i] = ref_list_create();
    }
    for (size_t ref = 0; ref < acirc_nrefs(c); ref++) {
        acirc_operation op = c->gates.gates[ref].op;
        if (op == OP_INPUT || op == OP_CONST)
            continue;
        acircref x = c->gates.gates[ref].args[0];
        acircref y = c->gates.gates[ref].args[1];
        ref_list_push(lists[x], ref);
        ref_list_push(lists[y], ref);
    }
    return lists;
}

void
ref_lists_free(ref_list **lists, const acirc *c)
{
    for (size_t i = 0; i < acirc_nrefs(c); i++) {
        ref_list_destroy(lists[i]);
    }
    free(lists);
}
