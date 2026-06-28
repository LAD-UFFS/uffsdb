#pragma once

#ifndef FTYPES
#include "types.h"
#endif

#ifndef FMACROS
#include "macros.h"
#endif

/* ----------------------------------------------------------------------------------------------
   Inicializa as variáveis do buffer (available_pages, bufferPool e head).
   Parametros: nenhum.
   Retorno: void.
---------------------------------------------------------------------------------------------- */
void bufferInit();

/* ----------------------------------------------------------------------------------------------
   Análoga à antiga getPage. Lê uma página do buffer e entrega as tuplas como resultado.
             Caso a página não esteja no buffer, ela é carregada do disco (via readBufferBlock).
   Parametros: esquema da tabela (tp_table), dados da tabela (fs_objects), número da página.
   Retorno: PageResult* com as tuplas da página, ou código de erro.
---------------------------------------------------------------------------------------------- */
PageResult *readBufferPage(tp_table *campos, struct fs_objects *table, int page, int *error_value);

/* ----------------------------------------------------------------------------------------------
   Análoga à antiga getBlock. Procura a página "id" da tabela na lista encadeada do
             buffer (bufferheader correspondente). Se não encontrar, lê a página do disco,
             ocupa um slot livre do bufferPool e insere na lista (mantendo ordenação por page_id).
             Se não houver slot livre, aborta o programa.
   Parametros: id da página, dados da tabela (fs_objects).
   Retorno: tp_bufferpage* apontando para a página dentro do bufferPool.
---------------------------------------------------------------------------------------------- */
tp_page *readBufferBlock(unsigned int id, struct fs_objects *table, int *error_value);

/* ----------------------------------------------------------------------------------------------
   Análoga à antiga writeBufferToDisk. Garante que a página esteja no buffer
             (chamando readBufferBlock caso ainda não esteja), escreve os dados na página do
             buffer e, por ser write-through, escreve imediatamente o mesmo conteúdo no disco.
   Parametros: página a ser escrita (tp_bufferpage), dados da tabela (fs_objects).
   Retorno: void.
---------------------------------------------------------------------------------------------- */
void writeToBuffer(tp_bufferpage *page, struct fs_objects *table);