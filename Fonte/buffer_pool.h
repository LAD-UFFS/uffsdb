#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <stdbool.h>
#include "memoryContext.h"

#ifndef FTYPES
  #include "types.h"
#endif

#ifndef FMACROS
   #include "macros.h"
#endif

/* Funções do Buffer Pool */
BufferPool* BP_Init(unsigned int num_frames, unsigned int page_int_size);
void BP_Destroy(BufferPool* pool);
tp_buffer* BP_FindPage(BufferPool* pool, unsigned int page_id);
tp_buffer* BP_FindPageByFile(BufferPool* pool, unsigned int page_id, const char* filename);
tp_buffer* BP_GetFreeFrame(BufferPool* pool);
tp_buffer* BP_SelectVictim(BufferPool* pool);
void BP_UpdateAccess(BufferPool* pool, tp_buffer* frame);

#endif /* BUFFER_POOL_H */
