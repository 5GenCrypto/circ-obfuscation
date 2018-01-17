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
    encoding *xx = x->x;
    encoding *yx = y->x;
    encoding *xu = x->u;
    encoding *yu = y->u;
    if (switches) {
        xx = encoding_copy(vt, pp_vt, pp, xx);
        yx = encoding_copy(vt, pp_vt, pp, yx);
        xu = encoding_copy(vt, pp_vt, pp, xu);
        yu = encoding_copy(vt, pp_vt, pp, yu);
        if (clt_pl_elem_level(xx->enc) != clt_pl_elem_level(yx->enc)) {
            if (clt_pl_elem_level(xx->enc) < clt_pl_elem_level(yx->enc))
                clt_pl_elem_switch(xx->enc, pp->pp, xx->enc, switches[0]);
            else
                clt_pl_elem_switch(yx->enc, pp->pp, yx->enc, switches[0]);
        }
        if (clt_pl_elem_level(xu->enc) != clt_pl_elem_level(yu->enc)) {
            if (clt_pl_elem_level(xu->enc) < clt_pl_elem_level(yu->enc))
                clt_pl_elem_switch(xu->enc, pp->pp, xu->enc, switches[0]);
            else
                clt_pl_elem_switch(yu->enc, pp->pp, yu->enc, switches[0]);
        }
    }
    encoding_mul(vt, pp_vt, rop->x, xx, yx, pp);
    encoding_mul(vt, pp_vt, rop->u, xu, yu, pp);
    if (switches) {
        clt_pl_elem_switch(rop->x->enc, pp->pp, rop->x->enc, switches[1]);
        clt_pl_elem_switch(rop->u->enc, pp->pp, rop->u->enc, switches[1]);
        encoding_free(vt, xx);
        encoding_free(vt, yx);
        encoding_free(vt, xu);
        encoding_free(vt, yu);
    }
    return OK;
}


static int
wire_op(const encoding_vtable *vt, const pp_vtable *pp_vt,
        const public_params *pp, wire_t *rop, const wire_t *x, const wire_t *y,
        switch_state_t **switches,
        int (*op)(const encoding_vtable *, const pp_vtable *, encoding *,
                  const encoding *, const encoding *, const public_params *))
{
    encoding *tmp;
    encoding *xx = x->x;
    encoding *yx = y->x;
    encoding *xu = x->u;
    encoding *yu = y->u;

    tmp = encoding_new(vt, pp_vt, pp);
    if (switches) {
        xx = encoding_copy(vt, pp_vt, pp, xx);
        yx = encoding_copy(vt, pp_vt, pp, yx);
        xu = encoding_copy(vt, pp_vt, pp, xu);
        yu = encoding_copy(vt, pp_vt, pp, yu);
        if (clt_pl_elem_level(xu->enc) != clt_pl_elem_level(yu->enc)) {
            if (clt_pl_elem_level(xu->enc) < clt_pl_elem_level(yu->enc))
                clt_pl_elem_switch(xu->enc, pp->pp, xu->enc, switches[0]);
            else
                clt_pl_elem_switch(yu->enc, pp->pp, yu->enc, switches[0]);
        }
        if (clt_pl_elem_level(xx->enc) != clt_pl_elem_level(yu->enc)) {
            if (clt_pl_elem_level(xx->enc) < clt_pl_elem_level(yu->enc))
                clt_pl_elem_switch(xx->enc, pp->pp, xx->enc, switches[0]);
            else
                clt_pl_elem_switch(yu->enc, pp->pp, yu->enc, switches[0]);
        }
        if (clt_pl_elem_level(xu->enc) != clt_pl_elem_level(yx->enc)) {
            if (clt_pl_elem_level(xu->enc) < clt_pl_elem_level(yx->enc))
                clt_pl_elem_switch(xu->enc, pp->pp, xu->enc, switches[0]);
            else
                clt_pl_elem_switch(yx->enc, pp->pp, yx->enc, switches[0]);
        }
    }
    encoding_mul(vt, pp_vt, rop->u, xu, yu, pp);
    encoding_mul(vt, pp_vt, tmp,    xx, yu, pp);
    encoding_mul(vt, pp_vt, rop->x, yx, xu, pp);
    if (switches) {
        clt_pl_elem_switch(rop->u->enc, pp->pp, rop->u->enc, switches[1]);
        clt_pl_elem_switch(tmp->enc,    pp->pp, tmp->enc,    switches[1]);
        clt_pl_elem_switch(rop->x->enc, pp->pp, rop->x->enc, switches[1]);
        encoding_free(vt, xx);
        encoding_free(vt, yx);
        encoding_free(vt, xu);
        encoding_free(vt, yu);
    }
    op(vt, pp_vt, rop->x, tmp, rop->x, pp);
    encoding_free(vt, tmp);
    return OK;
}

int
wire_add(const encoding_vtable *vt, const pp_vtable *pp_vt,
         const public_params *pp, wire_t *rop, const wire_t *x, const wire_t *y,
         switch_state_t **switches)
{
    return wire_op(vt, pp_vt, pp, rop, x, y, switches, encoding_add);
}

int
wire_sub(const encoding_vtable *vt, const pp_vtable *pp_vt,
         const public_params *pp, wire_t *rop, const wire_t *x, const wire_t *y,
         switch_state_t **switches)
{
    return wire_op(vt, pp_vt, pp, rop, x, y, switches, encoding_sub);
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
