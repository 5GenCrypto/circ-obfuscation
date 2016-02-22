#ifndef __LINKEDLIST__
#define __LINKEDLIST__

#include <stdlib.h>

struct list_node {
    void *data;
    struct list_node *next;
};
typedef struct list_node list_node;

typedef struct {
    list_node *head;
} list;

void list_init( list *x );
void list_clear( list *xs );
void list_add_node( list *existing, void *data );

#endif
