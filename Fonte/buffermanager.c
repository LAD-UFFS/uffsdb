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

//zera o estado do buffer na inicialização do banco.
void bufferInit() {
    available_pages = PAGES;
    memset(bufferPool, 0, sizeof(bufferPool));
    head = NULL;
}


bufferheader *findBufferHeader(char table_name[]) {
    bufferheader *aux = head;

    if (head == NULL) { // se não há nenhuma tabela no buffer ainda
        head = (bufferheader *)uffsllocType(sizeof(bufferheader), PERMANENT);
        head->next = NULL;
        head->table_list_head = NULL;
        strcpy(head->table_name, table_name);
        head->table_name[TAMANHO_NOME_ARQUIVO + 1] = '\0';
        return head;
    }
    // percorre os bufferheaders para encontrar o bufferheader da tabela
    bufferheader *prev = NULL;
    while (aux != NULL) {
        if (strcmp(aux->table_name, table_name) == 0) {
            return aux;
        }
        prev = aux;
        aux = aux->next;
    }

    // Não encontrado: cria um novo header no final da lista
    bufferheader *bh = (bufferheader *)uffsllocType(sizeof(bufferheader), PERMANENT);
    bh->next = NULL;
    bh->table_list_head = NULL;
    strcpy(bh->table_name, table_name);
    prev->next = bh;
    return bh;
}

tp_page *readBufferPage(unsigned int id, struct fs_objects *table, int *error_value) {
    *error_value = SUCCESS;  // nenhum erro (ainda)
    bufferheader *bh = findBufferHeader(table->nome);
    tp_bufferpage *bp = bh->table_list_head;

    if(bp != NULL) {
        while (bp->next != NULL && bp->next->page->id <= id) {  // procura entre as páginas da tabela por uma página com id <= id desejado
            bp = bp->next;
        } if (bp->page->id == id) {   // página já encontrada no buffer
            return bp->page;
        }
    }

    // Se a função não retornou, a página não estava no buffer (PAGE FAULT)
    if(available_pages > 0) {
        /* Lendo a página do disco */
        // DEBUG_PRINT("\nLoading page %d from %s", id, table->nome);
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
        /* Concluída a leitura da página do disco */

        /* Adicionando nova página ao buffer */
        int new_bp_index = PAGES - available_pages;
        DEBUG_PRINT("Adicionando %dª página ao buffer", new_bp_index + 1);
        available_pages--;

        // Estas informações são meio inúteis, pois não há como trocar páginas no buffer (pc não importa)
        //      e o buffer é um write-through buffer (db não importa)
        // bufferPool[new_bp_index].db = 0;
        // bufferPool[new_bp_index].pc = 1;

        bufferPool[new_bp_index].page = p;
        if(bp == NULL) {    // a única forma de bufferPage ser NULL é se o head da lista de páginas da tabela for NULL
            bufferPool[new_bp_index].next = NULL;
            bh->table_list_head = &bufferPool[new_bp_index];
        } else if (p->id < bh->table_list_head->page->id) {    // se id < id da página cabeça, muda o head
            bufferPool[new_bp_index].next = bh->table_list_head;
            bh->table_list_head = &bufferPool[new_bp_index];
        } else { 
            bufferPool[new_bp_index].next = bp->next;
            bp->next = &bufferPool[new_bp_index];
        }
        /* Concluída a adição da nova página ao buffer */
        return p;
    } else {
        printf("ERROR: buffer has no more pages available\n");
        *error_value = ERRO_BUFFER_CHEIO;
        return NULL;
    }
}

/* ----------------------------------------------------------------------------------------------

   Le uma pagina do buffer e devolve as tuplas validas organizadas para uso do Banco.
   Caso a pagina nao esteja no buffer ela é carregada do disco com a funcao readBufferPage

   Parametros:
     campos       - esquema da tabela (lista de tp_table, um nó por campo)
     table        - dados da tabela (fs_objects: nome do arquivo, qtdCampos, etc.)
     page         - número da página da tabela que se quer ler (id da página)
     error_value  - saída: SUCCESS se tudo correu bem, ou o código de erro correspondente
---------------------------------------------------------------------------------------------- */
PageResult *readBufferTuples(tp_table *campos, struct fs_objects *table, int page, int *error_value){

    *error_value = SUCCESS;

    if (page < 0) {
        printf("Erro: página inválida.\n\n");
        return NULL;
    }

    // garante que a página esteja no buffer; se não estiver, readBufferPage carrega do disco
    // (e já seta error_value internamente caso algo dê errado: arquivo não aberto, buffer cheio etc.)
    tp_page *p = readBufferPage((unsigned int) page, table, error_value);
    if (p == NULL) {
        return NULL; // error_value já foi setado dentro de readBufferPage
    }

    if (p->position == 0) {
        return NULL;
    }

    // aloca o vetor de tuplas a partir do nrec já conhecido da página
    // (nrec conta só tuplas válidas; tuplas deletadas ocupam espaço em "data" mas não em nrec)
    tupla *tuplas = (tupla *)uffslloc(sizeof(tupla) * p->nrec);
    if (!tuplas) {
        printf("ERROR: Memory allocation failed.\n\n");
        return NULL;
    }

    // buffer auxiliar: guarda, por campo da tupla atual, se o valor é nulo (1) ou não (0)
    char *nullos = (char *)uffslloc(table->qtdCampos * sizeof(char));
    if (!nullos) {
        printf("ERROR: Memory allocation failed.\n\n");
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

void writeToDisk(tp_page *page, struct fs_objects *table) {
    if (page == NULL) {
        printf("ERROR: writeToDisk received NULL page\n");
        return;
    }
 
    int error_value = SUCCESS;
 
    /* Tenta garantir que a página esteja no buffer */
    tp_page *pool_page = readBufferPage(page->id, table, &error_value);
 
    if (pool_page != NULL && pool_page != page) {
        /* Página carregada do disco (slot novo, conteúdo desatualizado):
           copia os dados modificados do chamador para o slot do pool */
        memcpy(pool_page, page, sizeof(tp_page));
    }
    /* Se pool_page == page: o chamador já trabalhou direto no slot do pool,
       nada a copiar. Se pool_page == NULL: buffer cheio, escrevemos só no disco. */
 
    /* Write-through: persiste imediatamente no disco */
    char filename[LEN_DB_NAME_IO];
    strcpy(filename, connected.db_directory);
    strcat(filename, table->nArquivo);
 
    FILE *fd = fopen(filename, "r+b");
    if (!fd) {
        printf("ERROR: writeToDisk failed to open %s\n", filename);
        return;
    }
 
    tp_page *to_write = (pool_page != NULL) ? pool_page : page;
    long int pos = (long int)to_write->id * sizeof(tp_page);
    fseek(fd, pos, SEEK_SET);
    fwrite(to_write, sizeof(tp_page), 1, fd);
    fclose(fd);
}

tp_page* initPage(unsigned int id){
    tp_page *page = uffslloc(sizeof(tp_page));

    if (page == NULL) {
        printf("ERROR: Memory allocation failed.\n\n");
        return NULL;
    }

    page->id = id;
    return page;
}