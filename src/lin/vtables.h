#ifndef __LIN__VTABLES_H__
#define __LIN__VTABLES_H__

#include "../mmap.h"

const pp_vtable *
lin_get_pp_vtable(const mmap_vtable *const mmap);

const sp_vtable *
lin_get_sp_vtable(const mmap_vtable *const mmap);

const encoding_vtable *
lin_get_encoding_vtable(const mmap_vtable *const mmap);

#endif
