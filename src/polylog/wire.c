#include "wire.h"
#include "../util.h"

#include <clt_pl.h>

struct wire_t {
    encoding *x;
    encoding *u;
    bool my_x;
    bool my_u;
};

encoding *
wire_x(wire_t *w)
{
    return w->x;
}

encoding *
wire_u(wire_t *w)
{
    return w->u;
}

wire_t *
wire_new(const encoding_vtable *vt, const pp_vtable *pp_vt, const public_params *pp)
{
    wire_t *wire;

    wire = calloc(1, sizeof wire[0]);
    wire->x = encoding_new(vt, pp_vt, pp);
    wire->u = encoding_new(vt, pp_vt, pp);
    return wire;
}

void
wire_free(const encoding_vtable *vt, wire_t *w)
{
    encoding_free(vt, w->x);
    encoding_free(vt, w->u);
    free(w);
}

int
wire_set(const encoding_vtable *vt, wire_t *rop, wire_t *w)
{
    encoding_set(vt, rop->x, w->x);
    encoding_set(vt, rop->u, w->u);
    return OK;
}

int
wire_mul(const encoding_vtable *vt, const pp_vtable *pp_vt,
         const public_params *pp, wire_t *rop, const wire_t *x, const wire_t *y,
         switch_state_t **switches)
{
    if (switches) {
        clt_elem_t *xx = x->x->enc;
        clt_elem_t *yx = y->x->enc;
        clt_elem_t *xu = x->u->enc;
        clt_elem_t *yu = y->u->enc;
        if (clt_pl_elem_level(xx) != clt_pl_elem_level(yx)) {
            if (clt_pl_elem_level(xx) < clt_pl_elem_level(yx))
                clt_pl_elem_switch(xx, pp->pp, xx, switches[0]);
            else
                clt_pl_elem_switch(yx, pp->pp, yx, switches[0]);
        }
        if (clt_pl_elem_level(xu) != clt_pl_elem_level(yu)) {
            if (clt_pl_elem_level(xu) < clt_pl_elem_level(yu))
                clt_pl_elem_switch(xu, pp->pp, xu, switches[0]);
            else
                clt_pl_elem_switch(yu, pp->pp, yu, switches[0]);
        }
    }
    encoding_mul(vt, pp_vt, rop->x, x->x, y->x, pp);
    encoding_mul(vt, pp_vt, rop->u, x->u, y->u, pp);
    if (switches) {
        clt_pl_elem_switch(rop->x->enc, pp->pp, rop->x->enc, switches[1]);
        clt_pl_elem_switch(rop->u->enc, pp->pp, rop->u->enc, switches[1]);
    }
    return OK;
}

int
wire_add(const encoding_vtable *vt, const pp_vtable *pp_vt,
         const public_params *pp, wire_t *rop, const wire_t *x, const wire_t *y,
         switch_state_t **switches)
{
    encoding *tmp;

    tmp = encoding_new(vt, pp_vt, pp);
    if (switches) {
        clt_elem_t *xx = x->x->enc;
        clt_elem_t *yx = y->x->enc;
        clt_elem_t *xu = x->u->enc;
        clt_elem_t *yu = y->u->enc;
        if (clt_pl_elem_level(xu) != clt_pl_elem_level(yu)) {
            if (clt_pl_elem_level(xu) < clt_pl_elem_level(yu))
                clt_pl_elem_switch(xu, pp->pp, xu, switches[0]);
            else
                clt_pl_elem_switch(yu, pp->pp, yu, switches[0]);
        }
        if (clt_pl_elem_level(xx) != clt_pl_elem_level(yu)) {
            if (clt_pl_elem_level(xx) < clt_pl_elem_level(yu))
                clt_pl_elem_switch(xx, pp->pp, xx, switches[0]);
            else
                clt_pl_elem_switch(yu, pp->pp, yu, switches[0]);
        }
        if (clt_pl_elem_level(xu) != clt_pl_elem_level(yx)) {
            if (clt_pl_elem_level(xu) < clt_pl_elem_level(yx))
                clt_pl_elem_switch(xu, pp->pp, xu, switches[0]);
            else
                clt_pl_elem_switch(yx, pp->pp, yx, switches[0]);
        }
    }
    encoding_mul(vt, pp_vt, rop->u, x->u, y->u, pp);
    encoding_mul(vt, pp_vt, tmp,    x->x, y->u, pp);
    encoding_mul(vt, pp_vt, rop->x, y->x, x->u, pp);
    if (switches) {
        clt_pl_elem_switch(rop->u->enc, pp->pp, rop->u->enc, switches[1]);
        clt_pl_elem_switch(tmp->enc,    pp->pp, tmp->enc,    switches[1]);
        clt_pl_elem_switch(rop->x->enc, pp->pp, rop->x->enc, switches[1]);
    }
    encoding_add(vt, pp_vt, rop->x, tmp, rop->x, pp);
    encoding_free(vt, tmp);
    return OK;
}

int
wire_sub(const encoding_vtable *vt, const pp_vtable *pp_vt,
         const public_params *pp, wire_t *rop, const wire_t *x, const wire_t *y,
         switch_state_t **switches)
{
    encoding *tmp;

    tmp = encoding_new(vt, pp_vt, pp);
    if (switches) {
        clt_elem_t *xx = x->x->enc;
        clt_elem_t *yx = y->x->enc;
        clt_elem_t *xu = x->u->enc;
        clt_elem_t *yu = y->u->enc;
        if (clt_pl_elem_level(xu) != clt_pl_elem_level(yu)) {
            if (clt_pl_elem_level(xu) < clt_pl_elem_level(yu))
                clt_pl_elem_switch(xu, pp->pp, xu, switches[0]);
            else
                clt_pl_elem_switch(yu, pp->pp, yu, switches[0]);
        }
        if (clt_pl_elem_level(xx) != clt_pl_elem_level(yu)) {
            if (clt_pl_elem_level(xx) < clt_pl_elem_level(yu))
                clt_pl_elem_switch(xx, pp->pp, xx, switches[0]);
            else
                clt_pl_elem_switch(yu, pp->pp, yu, switches[0]);
        }
        if (clt_pl_elem_level(xu) != clt_pl_elem_level(yx)) {
            if (clt_pl_elem_level(xu) < clt_pl_elem_level(yx))
                clt_pl_elem_switch(xu, pp->pp, xu, switches[0]);
            else
                clt_pl_elem_switch(yx, pp->pp, yx, switches[0]);
        }
    }
    encoding_mul(vt, pp_vt, rop->u, x->u, y->u, pp);
    encoding_mul(vt, pp_vt, tmp,    x->x, y->u, pp);
    encoding_mul(vt, pp_vt, rop->x, y->x, x->u, pp);
    if (switches) {
        clt_pl_elem_switch(rop->u->enc, pp->pp, rop->u->enc, switches[1]);
        clt_pl_elem_switch(tmp->enc,    pp->pp, tmp->enc,    switches[1]);
        clt_pl_elem_switch(rop->x->enc, pp->pp, rop->x->enc, switches[1]);
    }
    encoding_sub(vt, pp_vt, rop->x, tmp, rop->x, pp);
    encoding_free(vt, tmp);
    return OK;
}

wire_t *
wire_fread(const encoding_vtable *vt, FILE *fp)
{
    wire_t *w;

    w = calloc(1, sizeof w[0]);
    w->x = encoding_fread(vt, fp);
    w->u = encoding_fread(vt, fp);
    return w;
}

int
wire_fwrite(const encoding_vtable *vt, const wire_t *w, FILE *fp)
{
    encoding_fwrite(vt, w->x, fp);
    encoding_fwrite(vt, w->u, fp);
    return OK;
}
