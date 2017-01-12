#pragma once

#include <acirc.h>
#include <pthread.h>

typedef struct ref_list_node {
    acircref ref;
    struct ref_list_node *next;
} ref_list_node;

typedef struct {
    ref_list_node *first;
    pthread_mutex_t *lock;
} ref_list;

ref_list *ref_list_create(void);
void ref_list_destroy(ref_list *list);
void ref_list_push(ref_list *list, acircref ref);
