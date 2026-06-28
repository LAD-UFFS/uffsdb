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

#include "buffermanager.h"

static int available_pages = PAGES;          // Quantidade de slots livres no bufferPool
static tp_bufferpage bufferPool[PAGES]; // Array estático de páginas do buffer
static bufferheader *head;           // Lista de tabelas presentes no buffer

bufferheader *findBufferHeader(char* table_name) {
    bufferheader *aux = head;
    if (head == NULL) { // if there are no tables in the buffer already
        // create new head
        head = (bufferheader *)uffslloc(sizeof(bufferheader));
        head->next = NULL;
        strcpy(head->table_name, table_name);

        return head;
    } else {    //search through the bufferheaders to find the bufferheader from the table
        do {
            if (strcmp(aux->table_name, table_name) == 0) { //Table header from table found
                return aux;
            } else { 
                aux = aux->next;
            }
        } while(aux->next != NULL);
        if (strcmp(aux->table_name, table_name) == 0) { //Table header from table found (in the last element of the linked list)
            return aux;
        } else {    //Table header doesn't exist, so create a new one at the end of the linked list
            bufferheader *bh = (bufferheader *)uffslloc(sizeof(bufferheader));
            bh->next = NULL;
            strcpy(bh->table_name, table_name);
            aux->next = bh;

            return bh;
        }
    }   
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
            tp_page* p = uffslloc(sizeof(tp_page));
            fread(p, sizeof(tp_page), 1, fd);

            int new_bp_index = PAGES - available_pages;
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
            tp_page* p = uffslloc(sizeof(tp_page));
            fread(p, sizeof(tp_page), 1, fd);

            int new_bp_index = PAGES - available_pages;
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

