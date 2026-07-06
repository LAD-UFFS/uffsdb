#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <stdio.h>
#include "disk_manager.h"
#include "buffer_pool.h"
#include "memoryContext.h"

#ifndef FTYPES
  #include "types.h"
#endif

/*
 * O Buffer Manager agora opera sem manter um FILE* global.
 * Cada operação recebe o nome do arquivo da tabela, permitindo que o pool
 * sirva múltiplas tabelas simultaneamente.
 */

BufferManager* BM_Init(unsigned int num_frames, unsigned int page_size);
void           BM_Destroy(BufferManager* bm);

/* Obtém uma página pelo par (filename, page_id). Carrega do disco se necessário. */
tp_buffer*     BM_GetPage(BufferManager* bm, const char* filename, unsigned int page_id);

/* Carrega uma página do disco para um frame específico. */
int            BM_LoadPage(BufferManager* bm, const char* filename, unsigned int page_id, tp_buffer* target_frame);

/* Marca uma página como suja (dirty). */
void           BM_MarkDirty(BufferManager* bm, const char* filename, unsigned int page_id);

/* Incrementa o pin_count de uma página. */
void           BM_PinPage(BufferManager* bm, const char* filename, unsigned int page_id);

/* Decrementa o pin_count de uma página. */
void           BM_UnpinPage(BufferManager* bm, const char* filename, unsigned int page_id);

/* Descarrega uma página suja para o disco. */
int            BM_FlushPage(BufferManager* bm, const char* filename, unsigned int page_id);

/* Descarrega todas as páginas sujas do pool para o disco. */
void           BM_FlushAll(BufferManager* bm);

#endif /* BUFFER_MANAGER_H */
