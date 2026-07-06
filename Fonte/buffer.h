#ifndef BUFFER_COMPAT_H
#define BUFFER_COMPAT_H

#define FBUFFER 1 // flag controlar os includes

#include "buffer_manager.h"
#include "page.h"
#include "record_manager.h"
#include "memoryContext.h" 


#ifndef FMACROS // garante que macros.h não seja reincluída
   #include "macros.h"
#endif
//
#ifndef FTYPES // garante que types.h não seja reincluída
  #include "types.h"
#endif

#ifndef FMISC // garante que misc.h não seja reincluída
  #include "misc.h"
#endif


// Declaração de uma variável global para o BufferManager para que as funções de compatibilidade possam acessá-lo.
extern BufferManager *global_buffer_manager; // Será inicializado em BM_Init

// utiliza o Record Manager para obter e imprimir as tuplas
int printbufferpoll(tp_buffer *buffpoll, tp_table *s, struct fs_objects objeto, int num_page);

// inicializa o BufferManager e o BufferPool
tp_buffer* initBuffer(unsigned int id);

// obtém um tp_buffer do BufferManager
tp_buffer *getBlock(unsigned int id, char* filename);

// utiliza o Record Manager para obter as tuplas de uma página
PageResult *getPage(tp_table *campos, struct fs_objects objeto, int page);

//utiliza o Record Manager para deletar uma tupla, retorna um ponteiro para a tupla excluída
column * excluirTuplaBuffer(tp_buffer *buffer, tp_table *campos, struct fs_objects objeto, int page_id, int nTupla);

// utiliza o BufferManager e o módulo Page para obter uma tupla
char *getTupla(tp_table *campos, struct fs_objects objeto, int from);

// utiliza o módulo Page para escrever uma tupla em um tp_buffer
void setTupla(tp_buffer *buffer, char *tupla_data, int tam, int pos);

// utiliza o Record Manager para inserir uma tupla
int colocaTuplaBuffer(tp_buffer *buffer, int from, tp_table *campos, struct fs_objects objeto);

// utiliza o BufferManager para descarregar uma página
int writeBufferToDisk(tp_buffer *buffer, struct fs_objects *objeto);

// Funções auxiliares que podem ter sido definidas em outros headers do UFFSDB
extern int cabecalho(tp_table *s, int num_reg);
extern int drawline(tupla *t, tp_table *s, struct fs_objects objeto);
extern void strcpylower(char *dest, const char *src);

#endif // BUFFER_COMPAT_H
