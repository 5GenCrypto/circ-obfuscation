#pragma once

#include <acirc.h>

typedef struct ref_list_node {
    acircref ref;
    struct ref_list_node *next;
} ref_list_node;

typedef struct {
    ref_list_node *first;
} ref_list;

ref_list **
ref_lists_new(const acirc *c);
void
ref_lists_free(ref_list **lists, const acirc *c);
