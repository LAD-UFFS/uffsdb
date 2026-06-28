#define FBUFFERMANAGER 1

#ifndef FMACROS
#include "macros.h"
#endif

#ifndef FTYPES
#include "types.h"
#endif

tp_pagina *bm_getBlock(int id_tabela, int id_bloco, char *filename);

tp_pagina *bm_writeBufferToDisk(struct fs_objects *objeto);