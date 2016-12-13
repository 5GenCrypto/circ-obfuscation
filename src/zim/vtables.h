#ifndef __ZIM__VTABLES_H__
#define __ZIM__VTABLES_H__

#include "mmap.h"

pp_vtable *
zim_get_pp_vtable(const mmap_vtable *const mmap);

sp_vtable *
zim_get_sp_vtable(const mmap_vtable *const mmap);

encoding_vtable *
zim_get_encoding_vtable(const mmap_vtable *const mmap);

#endif
