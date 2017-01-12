#pragma once

#include "../mmap.h"

pp_vtable *
get_pp_vtable(const mmap_vtable *const mmap);

sp_vtable *
get_sp_vtable(const mmap_vtable *const mmap);

encoding_vtable *
get_encoding_vtable(const mmap_vtable *const mmap);
