#ifndef RECORD_MANAGER_H
#define RECORD_MANAGER_H

#include "buffer_manager.h"
#include "page.h"
#include "memoryContext.h"

#ifndef FTYPES
  #include "types.h"
#endif

#ifndef FMACROS
   #include "macros.h"
#endif

extern db_connected connected;

// Funções auxiliares 
int  printbufferpoll_adapted(PageResult *pr, tp_table *s, struct fs_objects objeto);
void addColumn(column **colList, column *c);

// Todas as funções do Record Manager recebem filename (caminho completo do
// arquivo da tabela) para que o BufferManager possa identificar as páginas
// corretamente em um pool compartilhado entre múltiplas tabelas

int RM_InsertRecord(BufferManager* bm, const char* filename, tp_table *s, struct fs_objects objeto, const tupla *nova_tupla);

int RM_DeleteRecord(BufferManager* bm, const char* filename, tp_table *s, struct fs_objects objeto, unsigned int page_id, unsigned int offset);

int RM_UpdateRecord(BufferManager* bm, const char* filename, tp_table *s, struct fs_objects objeto, unsigned int page_id, unsigned int offset, const tupla *tupla_atualizada);

PageResult* RM_SelectAllRecords(BufferManager* bm, const char* filename, tp_table *s, struct fs_objects objeto);

extern int tamTupla(tp_table *esquema, struct fs_objects objeto);

#endif /* RECORD_MANAGER_H */
