#ifndef __LZ__VTABLES_H__
#define __LZ__VTABLES_H__

#include "../mmap.h"

pp_vtable *
lz_get_pp_vtable(const mmap_vtable *const mmap);

sp_vtable *
lz_get_sp_vtable(const mmap_vtable *const mmap);

encoding_vtable *
lz_get_encoding_vtable(const mmap_vtable *const mmap);

#endif
