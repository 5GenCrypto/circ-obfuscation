#include "linked-list.h"

void list_init( list *xs ) {
    xs->head = NULL;
}

void list_clear( list *xs ) {
    list_node *tmp, *cur;
    cur = xs->head;
    while (cur->next != NULL) {
        tmp = cur->next;
        free(cur);
        cur = tmp;
    }
    free(cur);
}

void list_add_node( list *xs, void *data ) {
    list_node *tmp = malloc(sizeof(list_node));
    tmp->data = data;
    tmp->next = xs->head;
    xs->head  = tmp;
}
