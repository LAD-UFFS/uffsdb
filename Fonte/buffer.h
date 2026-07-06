#ifndef BUFFER_COMPAT_H
#define BUFFER_COMPAT_H

#define FBUFFER 1 // flag controlar os includes

#include "buffer_manager.h"
#include "page.h"
#include "record_manager.h"
#include "memoryContext.h" // Para uffsllocType e MemoryContextType


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

// --- Protótipos das Funções de Compatibilidade (do buffer.c original) ---

/**
 * @brief Wrapper para a função printbufferpoll original.
 *        Agora utiliza o Record Manager para obter e imprimir as tuplas.
 * @param buffpoll O ponteiro para o buffer pool (agora ignorado, usa BM).
 * @param s Metadados da tabela (tp_table).
 * @param objeto Informações do objeto (tabela) fs_objects.
 * @param num_page O ID da página a ser impressa (agora ignorado, pois RM_SelectAllRecords itera).
 * @return SUCCESS em caso de sucesso, ERRO_IMPRESSAO em caso de falha.
 */
int printbufferpoll(tp_buffer *buffpoll, tp_table *s, struct fs_objects objeto, int num_page);

/**
 * @brief Wrapper para a função initBuffer original.
 *        Agora inicializa o BufferManager e o BufferPool.
 * @param id O ID do buffer (agora ignorado, pois o BM gerencia o pool).
 * @return Um ponteiro para um tp_buffer (Frame) simulado, ou NULL em caso de falha.
 */
tp_buffer* initBuffer(unsigned int id);

/**
 * @brief Wrapper para a função getBlock original.
 *        Agora obtém um tp_buffer do BufferManager.
 * @param id O ID da página (bloco) a ser obtida.
 * @param filename O nome do arquivo (agora gerenciado pelo BufferManager).
 * @return Um ponteiro para o tp_buffer ou NULL em caso de erro.
 */
tp_buffer *getBlock(unsigned int id, char* filename);

/**
 * @brief Wrapper para a função getPage original.
 *        Agora utiliza o Record Manager para obter as tuplas de uma página.
 * @param campos Metadados dos campos da tabela.
 * @param objeto Informações do objeto (tabela) fs_objects.
 * @param page O ID da página.
 * @return Um ponteiro para PageResult contendo as tuplas, ou NULL em caso de erro.
 */
PageResult *getPage(tp_table *campos, struct fs_objects objeto, int page);

/**
 * @brief Wrapper para a função excluirTuplaBuffer original.
 *        Agora utiliza o Record Manager para deletar uma tupla.
 * @param buffer O ponteiro para o buffer (agora ignorado, usa BM).
 * @param campos Metadados da tabela (tp_table).
 * @param objeto Informações do objeto (tabela) fs_objects.
 * @param page_id O ID da página onde a tupla está localizada.
 * @param nTupla O índice da tupla a ser deletada (requer adaptação para offset).
 * @return Um ponteiro para a tupla excluída (se necessário), ou NULL em caso de erro.
 */
column * excluirTuplaBuffer(tp_buffer *buffer, tp_table *campos, struct fs_objects objeto, int page_id, int nTupla);

/**
 * @brief Wrapper para a função getTupla original.
 *        Agora utiliza o BufferManager e o módulo Page para obter uma tupla.
 * @param campos Metadados da tabela (tp_table).
 * @param objeto Informações do objeto (tabela) fs_objects.
 * @param from O índice da tupla (agora o offset dentro da página).
 * @return Um ponteiro para a tupla (char*), ou ERRO_DE_LEITURA.
 */
char *getTupla(tp_table *campos, struct fs_objects objeto, int from);

/**
 * @brief Wrapper para a função setTupla original.
 *        Agora utiliza o módulo Page para escrever uma tupla em um tp_buffer.
 * @param buffer O ponteiro para o buffer (agora ignorado, usa BM).
 * @param tupla_data A tupla em formato char* (precisa ser convertida para tupla*).
 * @param tam O tamanho da tupla (agora calculado por tamTupla).
 * @param pos O ID da página.
 */
void setTupla(tp_buffer *buffer, char *tupla_data, int tam, int pos);

/**
 * @brief Wrapper para a função colocaTuplaBuffer original.
 *        Agora utiliza o Record Manager para inserir uma tupla.
 * @param buffer O ponteiro para o buffer (agora ignorado, usa BM).
 * @param from O ID da página de origem da tupla (agora o Record Manager decide onde inserir).
 * @param campos Metadados da tabela (tp_table).
 * @param objeto Informações do objeto (tabela) fs_objects.
 * @return SUCCESS em caso de sucesso, ou código de erro.
 */
int colocaTuplaBuffer(tp_buffer *buffer, int from, tp_table *campos, struct fs_objects objeto);

/**
 * @brief Wrapper para a função writeBufferToDisk original.
 *        Agora utiliza o BufferManager para descarregar uma página.
 * @param buffer O ponteiro para o buffer (agora ignorado, usa BM).
 * @param objeto Informações do objeto (tabela) fs_objects.
 * @return 1 para sucesso, 0 para falha.
 */
int writeBufferToDisk(tp_buffer *buffer, struct fs_objects *objeto);

// Funções auxiliares que podem ter sido definidas em outros headers do UFFSDB
extern int cabecalho(tp_table *s, int num_reg);
extern int drawline(tupla *t, tp_table *s, struct fs_objects objeto);
extern void strcpylower(char *dest, const char *src);

#endif // BUFFER_COMPAT_H
