#include "linked-list.h"

void list_add_node( list *xs, void *data ) {
    list_node *tmp = malloc(sizeof(list_node));
    tmp->data = data;
    tmp->next = xs->head;
    xs->head  = tmp;
}
