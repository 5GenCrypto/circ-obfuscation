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

ref_list *
ref_list_create(void)
{
    ref_list *list = my_calloc(1, sizeof(ref_list));
    list->first = NULL;
    return list;
}

void
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

void
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
