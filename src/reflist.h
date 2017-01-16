#pragma once

#include <acirc.h>

typedef struct ref_list_node {
    acircref ref;
    struct ref_list_node *next;
} ref_list_node;

typedef struct {
    ref_list_node *first;
} ref_list;

ref_list *ref_list_create(void);
void ref_list_destroy(ref_list *list);
void ref_list_push(ref_list *list, acircref ref);
