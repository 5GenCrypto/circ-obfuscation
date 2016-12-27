#ifndef __AB__VTABLES_H__
#define __AB__VTABLES_H__

#include "../mmap.h"

const pp_vtable *
ab_get_pp_vtable(const mmap_vtable *const mmap);

const sp_vtable *
ab_get_sp_vtable(const mmap_vtable *const mmap);

const encoding_vtable *
ab_get_encoding_vtable(const mmap_vtable *const mmap);

#endif
