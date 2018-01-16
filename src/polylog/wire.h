#pragma once

#include "../mmap.h"

#include <clt_pl.h>

typedef struct wire_t wire_t;

encoding * wire_x(wire_t *w);
encoding * wire_u(wire_t *w);

wire_t *
wire_new(const encoding_vtable *vt, const pp_vtable *pp_vt, const public_params *pp);
void
wire_free(const encoding_vtable *vt, wire_t *w);
int
wire_set(const encoding_vtable *vt, wire_t *rop, wire_t *w);
int
wire_mul(const encoding_vtable *vt, const pp_vtable *pp_vt,
         const public_params *pp, wire_t *rop, const wire_t *x, const wire_t *y,
         switch_state_t **switches);
int
wire_add(const encoding_vtable *vt, const pp_vtable *pp_vt,
         const public_params *pp, wire_t *rop, const wire_t *x, const wire_t *y,
         switch_state_t **switches);
int
wire_sub(const encoding_vtable *vt, const pp_vtable *pp_vt,
         const public_params *pp, wire_t *rop, const wire_t *x, const wire_t *y,
         switch_state_t **switches);
wire_t *
wire_fread(const encoding_vtable *vt, FILE *fp);
int
wire_fwrite(const encoding_vtable *vt, const wire_t *w, FILE *fp);
