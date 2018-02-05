#pragma once

#include "mife.h"

int
mife_ct_write(const mife_vtable *vt, mife_ct_t *ct, const char *ctname);
mife_ct_t *
mife_ct_read(const mmap_vtable *mmap, const mife_vtable *vt,
             const mife_ek_t *ek, const char *ctname);
