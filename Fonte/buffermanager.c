#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memoryContext.h"

#ifndef FTYPES
#include "types.h"
#endif

#ifndef FMACROS
#include "macros.h"
#endif

#include "dictionary.h" // tamTupla

#include "buffermanager.h"

static int available_pages = PAGES;          // Quantidade de slots livres no bufferPool
static tp_bufferpage bufferPool[PAGES]; // Array estático de páginas do buffer
static bufferheader *head;           // Lista de tabelas presentes no buffer

/* ----------------------------------------------------------------------------------------------
   Verifica se uma tupla foi marcada como deletada a partir do seu primeiro byte (o "header"
   da tupla). Se for 1, a tupla foi deletada e deve ser ignorada na leitura.
---------------------------------------------------------------------------------------------- */
static int isDeleted(char *linha){
    return linha[0];
}

bufferheader *findBufferHeader(char table_name[]) {
    bufferheader *aux = head;

    if (head == NULL) { // if there are no tables in the buffer already
        head = (bufferheader *)uffsllocType(sizeof(bufferheader), PERMANENT);
        head->next = NULL;
        head->table_list_head = NULL;
        strcpy(head->table_name, table_name);
        head->table_name[TAMANHO_NOME_ARQUIVO + 1] = '\0';
        return head;
    }
    // search through the bufferheaders to find the bufferheader for the table
    bufferheader *prev = NULL;
    while (aux != NULL) {
        if (strcmp(aux->table_name, table_name) == 0) {
            return aux;
        }
        prev = aux;
        aux = aux->next;
    }

    // Not found: create a new header at the end of the list
    bufferheader *bh = (bufferheader *)uffsllocType(sizeof(bufferheader), PERMANENT);
    bh->next = NULL;
    bh->table_list_head = NULL;
    strcpy(bh->table_name, table_name);
    prev->next = bh;
    return bh;
}

tp_page *readBufferBlock(unsigned int id, struct fs_objects *table, int *error_value) {
    *error_value = SUCCESS;  //no error (yet)
    bufferheader *bh = findBufferHeader(table->nome);
    
    tp_bufferpage *bp = bh->table_list_head;
    if (bp == NULL) {   //No pages from table in buffer already, add a new one as the head
        if(available_pages > 0) {
            // printf("\nAvailable pages: %d", available_pages);
            // printf("\nLoading page %d from %s", id, table->nome);
            char filename[LEN_DB_NAME_IO];
            strcpy(filename, connected.db_directory);
            strcat(filename, table->nArquivo);
            FILE *fd = fopen(filename, "r+");

            if (!fd) {
                printf("ERROR: failed to open %s", filename);
                *error_value = ERRO_ABRIR_ARQUIVO;
                return NULL;
            }

            long int pos = (long int)id * sizeof(tp_page);
            fseek(fd, pos, SEEK_SET);
            tp_page* p = (tp_page *)uffsllocType(sizeof(tp_page), PERMANENT);
            fread(p, sizeof(tp_page), 1, fd);

            int new_bp_index = PAGES - available_pages;
            DEBUG_PRINT("Adicionando %dª página ao buffer", new_bp_index);
            available_pages--;

            // These are kinda useless, as there isn't a way to change pages in the buffer and the buffer is a write-through buffer
            // bufferPool[new_bp_index].db = 0;
            // bufferPool[new_bp_index].pc = 1;

            bufferPool[new_bp_index].next = NULL;
            bufferPool[new_bp_index].page = p;

            bh->table_list_head = &bufferPool[new_bp_index];
            return p;
        } else {
            printf("\nERROR: buffer has no more pages available\n");
            *error_value = ERRO_BUFFER_CHEIO;
            return NULL;
        }
    }

    while (bp->next != NULL && bp->next->page->id <= id) {  //look through the pages from the table for a page id <= desired id
        bp = bp->next;
    }

    if (bp->page->id == id) {   //page found in buffer already
        return bp->page;
    } else {    //create a new page
        if(available_pages > 0) {
            // printf("\nAvailable pages: %d", available_pages);
            // printf("\nLoading page %d from %s", id, table->nome);
            char filename[LEN_DB_NAME_IO];
            strcpy(filename, connected.db_directory);
            strcat(filename, table->nArquivo);
            FILE *fd = fopen(filename, "r+");

            if (!fd) {
                printf("ERROR: failed to open %s", filename);
                *error_value = ERRO_ABRIR_ARQUIVO;
                return NULL;
            }

            long int pos = (long int)id * sizeof(tp_page);
            fseek(fd, pos, SEEK_SET);
            tp_page* p = (tp_page *)uffsllocType(sizeof(tp_page), PERMANENT);
            fread(p, sizeof(tp_page), 1, fd);

            int new_bp_index = PAGES - available_pages;
            DEBUG_PRINT("Adicionando %dª página ao buffer", new_bp_index);
            available_pages--;

            // These are kinda useless, as there isn't a way to change pages in the buffer and the buffer is a write-through buffer
            // bufferPool[new_bp_index].db = 0;
            // bufferPool[new_bp_index].pc = 1;

            bufferPool[new_bp_index].page = p;

            if (p->id < bh->table_list_head->page->id) {    //if id < head page id, change head
                bufferPool[new_bp_index].next = bh->table_list_head;
                bh->table_list_head = &bufferPool[new_bp_index];
            } else { 
                bufferPool[new_bp_index].next = bp->next;
                bp->next = &bufferPool[new_bp_index];
            }
            return p;
        } else {
            printf("ERROR: buffer has no more pages available\n");
            *error_value = ERRO_BUFFER_CHEIO;
            return NULL;
        }
    }
}

/* ----------------------------------------------------------------------------------------------

   Diferença em relação à versão anterior: agora readBufferBlock devolve um tp_page* direto
   (sem o "wrapper" tp_bufferpage) e comunica erros através de error_value, em vez de embutir
   o código de erro no valor de retorno. readBufferPage segue o mesmo padrão, propagando pra
   cima qualquer erro que vier de dentro do readBufferBlock (arquivo não encontrado, buffer
   cheio, etc.) em vez de mascarar tudo como um erro genérico de leitura.

   Parametros:
     campos       - esquema da tabela (lista de tp_table, um nó por campo)
     table        - dados da tabela (fs_objects: nome do arquivo, qtdCampos, etc.)
     page         - número da página da tabela que se quer ler (id da página)
     error_value  - saída: SUCCESS se tudo correu bem, ou o código de erro correspondente
---------------------------------------------------------------------------------------------- */
PageResult *readBufferPage(tp_table *campos, struct fs_objects *table, int page, int *error_value){

    *error_value = SUCCESS;

    if (page < 0) {
        *error_value = ERRO_PAGINA_INVALIDA;
        return NULL;
    }

    // garante que a página esteja no buffer; se não estiver, readBufferBlock carrega do disco
    // (e já seta error_value internamente caso algo dê errado: arquivo não aberto, buffer cheio etc.)
    tp_page *p = readBufferBlock((unsigned int) page, table, error_value);
    if (p == NULL) {
        return NULL; // error_value já foi setado dentro de readBufferBlock
    }

    if (p->position == 0) {
        return NULL;
    }

    // aloca o vetor de tuplas a partir do nrec já conhecido da página
    // (nrec conta só tuplas válidas; tuplas deletadas ocupam espaço em "data" mas não em nrec)
    tupla *tuplas = (tupla *)uffslloc(sizeof(tupla) * p->nrec);
    if (!tuplas) {
        *error_value = ERRO_DE_ALOCACAO;
        return NULL;
    }

    // buffer auxiliar: guarda, por campo da tupla atual, se o valor é nulo (1) ou não (0)
    char *nullos = (char *)uffslloc(table->qtdCampos * sizeof(char));
    if (!nullos) {
        *error_value = ERRO_DE_ALOCACAO;
        return NULL;
    }

    int indiceTupla = 0; // próxima posição livre em "tuplas" (só avança para tuplas válidas)
    int i = 0;            // posição (em bytes) sendo lida dentro de p->data

    while (i < (int) p->position){

        // byte de "deletado": se marcado, salta a tupla inteira e segue para a próxima
        if (isDeleted(p->data + i)){
            i += tamTupla(campos, *table);
            continue;
        }

        tuplas[indiceTupla].offset = i;
        tuplas[indiceTupla].ncols = table->qtdCampos;
        tuplas[indiceTupla].bufferPage = page; // página de origem, útil para update/delete depois

        i++; // avança o byte de deletado que já foi lido acima

        // copia de uma vez todos os bytes de "é nulo?" dos campos dessa tupla
        memcpy(nullos, p->data + i, table->qtdCampos);
        i += table->qtdCampos;

        // aloca uma coluna para cada campo do esquema da tabela
        tuplas[indiceTupla].column = (column *)uffslloc(sizeof(column) * table->qtdCampos);

        for (int ic = 0; ic < table->qtdCampos; ic++){
            column *c = &tuplas[indiceTupla].column[ic];

            c->tipoCampo = campos[ic].tipo;
            strcpy(c->nomeCampo, campos[ic].nome);

            if (nullos[ic]){
                // campo nulo: não existe valor para copiar de "data"
                c->valorCampo = COLUNA_NULL;
            } else {
                // copia o valor do campo direto da página em memória (sem nenhum acesso a disco)
                c->valorCampo = (char *)uffslloc(sizeof(char) * campos[ic].tam + 1);
                memcpy(c->valorCampo, p->data + i, campos[ic].tam);
                c->valorCampo[campos[ic].tam] = '\0';
            }

            i += campos[ic].tam;
        }

        indiceTupla++;
    }

    PageResult *pg = (PageResult *)uffslloc(sizeof(PageResult));
    pg->tuplas = tuplas;
    pg->nrec = indiceTupla;

    return pg;
}