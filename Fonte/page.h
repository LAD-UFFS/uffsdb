#ifndef PAGE_H
#define PAGE_H

#include "memoryContext.h"

#ifndef FTYPES
  #include "types.h"
#endif

#ifndef FMACROS
  #include "macros.h"
#endif

/* Macro auxiliar: verifica se o byte de deleção indica tupla deletada */
static inline int isDeleted(const char *ptr) {
    return (unsigned char)ptr[0] == 1;
}

/* Extrai uma única tupla de um frame na posição offset */
tupla*      PAGE_GetTupla(tp_buffer *page_buffer, unsigned int offset,
                           tp_table *campos, struct fs_objects objeto);

/* Extrai todas as tuplas válidas de um frame */
PageResult* PAGE_GetTuplasFromFrame(tp_buffer *page_buffer,
                                     tp_table *campos, struct fs_objects objeto);

/* Escreve uma tupla em um frame na posição offset */
int         PAGE_SetTupla(tp_buffer *page_buffer, unsigned int offset,
                           const tupla *nova_tupla, tp_table *campos,
                           struct fs_objects objeto);

extern int tamTupla(tp_table *esquema, struct fs_objects objeto);

#endif /* PAGE_H */
