#pragma once

#include <acirc.h>

typedef struct {
    size_t cur, max;
    acircref *refs;
} ref_list_node;

typedef struct {
    ref_list_node *refs;
} ref_list;

ref_list *
ref_list_new(const acirc *c);
void
ref_list_free(ref_list *lst, const acirc *c);
