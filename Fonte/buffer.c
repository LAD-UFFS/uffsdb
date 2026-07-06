#include "buffer.h"
#include "buffer_manager.h"
#include "page.h"
#include "record_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef FMACROS
   #include "macros.h"
#endif

#ifndef FTYPES
  #include "types.h"
#endif

#ifndef FMISC
  #include "misc.h"
#endif

#ifndef FDICTIONARY
  #include "dictionary.h"
#endif

/* Variável global do BufferManager — único, sem arquivo fixo */
BufferManager *global_buffer_manager = NULL;

/* -----------------------------------------------------------------------
 * initBuffer
 *
 * Inicializa o BufferManager global (se ainda não foi feito) e retorna
 * um frame livre do pool inicializado com zeros para a página `id`.
 * O código legado em sqlcommands.c usa o ponteiro retornado para escrever
 * dados diretamente em frame->data; por isso precisamos retornar um frame
 * real, não NULL.
 *
 * O parâmetro `filename` indica o arquivo da tabela que será associado
 * ao frame. Quando NULL, usa o arquivo padrão "uffsdb.dat" (compatibilidade).
 * ----------------------------------------------------------------------- */
tp_buffer* initBuffer(unsigned int id) {
    /* Inicializa o BM global uma única vez */
    if (global_buffer_manager == NULL) {
        global_buffer_manager = BM_Init(16, SIZE);
        if (global_buffer_manager == NULL) {
            fprintf(stderr, "ERROR: Failed to initialize global BufferManager.\n");
            return NULL;
        }
    }

    /*
     * Retorna um frame livre do pool para que o código legado possa
     * escrever diretamente em frame->data. O frame é marcado com id=`id`
     * e filename vazio (será preenchido antes do flush).
     */
    tp_buffer* frame = BP_GetFreeFrame(global_buffer_manager->pool);
    if (frame == NULL) {
        /* Sem frames livres: usa vítima LRU */
        frame = BP_SelectVictim(global_buffer_manager->pool);
        if (frame == NULL) {
            fprintf(stderr, "ERROR: No available frames in BufferPool.\n");
            return NULL;
        }
        /* Se a vítima estava suja, grava antes de reutilizar */
        if (frame->db && frame->filename[0] != '\0') {
            Disk_WritePageByName(frame->filename, frame->id,
                                  frame, global_buffer_manager->pool->page_size);
            global_buffer_manager->num_writes++;
        }
    }

    /* Inicializa o frame como uma página nova/vazia */
    frame->id       = id;
    frame->nrec     = 0;
    frame->position = 0;
    frame->db       = 0;
    frame->pc       = 1; /* pinado */
    frame->filename[0] = '\0'; /* arquivo será definido antes do flush */
    memset(frame->data, 0, global_buffer_manager->pool->page_size);
    BP_UpdateAccess(global_buffer_manager->pool, frame);

    return frame;
}

/* -----------------------------------------------------------------------
 * getBlock
 *
 * Obtém um frame do pool para a página `id` do arquivo `filename`.
 * Se a página já estiver no pool (cache hit), retorna o frame existente.
 * Se não estiver, carrega do disco (ou inicializa com zeros se for nova).
 * ----------------------------------------------------------------------- */
tp_buffer* getBlock(unsigned int id, char* filename) {
    if (global_buffer_manager == NULL) {
        global_buffer_manager = BM_Init(16, SIZE);
        if (global_buffer_manager == NULL) {
            fprintf(stderr, "ERROR: BufferManager not initialized. getBlock()\n");
            return NULL;
        }
    }

    if (filename == NULL || filename[0] == '\0') {
        fprintf(stderr, "ERROR: getBlock called with NULL/empty filename.\n");
        return NULL;
    }

    return BM_GetPage(global_buffer_manager, filename, id);
}

/* -----------------------------------------------------------------------
 * getPage
 *
 * Obtém as tuplas de uma página específica de uma tabela.
 * ----------------------------------------------------------------------- */
PageResult* getPage(tp_table *campos, struct fs_objects objeto, int page) {
    if (global_buffer_manager == NULL) {
        global_buffer_manager = BM_Init(16, SIZE);
        if (global_buffer_manager == NULL) {
            fprintf(stderr, "ERROR: Failed to initialize BufferManager in getPage()\n");
            return ERRO_PAGINA_INVALIDA;
        }
    }

    /* Monta o caminho completo do arquivo da tabela */
    char filepath[LEN_DB_NAME_IO * 2];
    snprintf(filepath, sizeof(filepath), "%s%s", connected.db_directory, objeto.nArquivo);

    tp_buffer* page_buffer = BM_GetPage(global_buffer_manager, filepath, (unsigned int)page);
    if (page_buffer == NULL) {
        fprintf(stderr, "ERROR: Failed to get page %d from BufferManager.\n", page);
        return ERRO_PAGINA_INVALIDA;
    }

    /*
     * Se position == 0, pode ser que a página foi recém lida do disco e
     * o metadado não foi restaurado. Recalcula varrendo os bytes do data.
     * O formato da tupla é: 1 byte deleção + qtdCampos bytes (bitmap) + campos.
     */
    if (page_buffer->position == 0) {
        int tuple_size = tamTupla(campos, objeto);
        if (tuple_size > 0) {
            unsigned int pos = 0;
            int nrec = 0;
            while (pos + (unsigned int)tuple_size <= SIZE) {
                /* Verifica se o byte de deleção é 0 (válido) ou 1 (deletado) */
                unsigned char del_byte = (unsigned char)page_buffer->data[pos];
                /* Se o byte é 0 ou 1, é uma tupla válida (não lixo) */
                /* Verifica se há algum dado não-nulo após o byte de deleção */
                int has_data = 0;
                for (int b = 0; b < tuple_size; b++) {
                    if ((unsigned char)page_buffer->data[pos + b] != 0) {
                        has_data = 1;
                        break;
                    }
                }
                if (!has_data) break; /* chegou ao fim dos dados */
                (void)del_byte;
                if (del_byte == 0) nrec++; /* conta apenas não-deletadas */
                pos += (unsigned int)tuple_size;
            }
            page_buffer->position = pos;
            page_buffer->nrec = nrec;
        }
    }

    /* Se a página está vazia (sem dados), não há tuplas */
    if (page_buffer->position == 0) {
        BM_UnpinPage(global_buffer_manager, filepath, (unsigned int)page);
        return NULL;
    }

    PageResult *pr = PAGE_GetTuplasFromFrame(page_buffer, campos, objeto);
    BM_UnpinPage(global_buffer_manager, filepath, (unsigned int)page);
    return pr;
}

/* -----------------------------------------------------------------------
 * printbufferpoll
 * ----------------------------------------------------------------------- */
int printbufferpoll(tp_buffer *buffpoll, tp_table *s, struct fs_objects objeto, int num_page) {
    if (global_buffer_manager == NULL) {
        global_buffer_manager = BM_Init(16, SIZE);
        if (global_buffer_manager == NULL) {
            fprintf(stderr, "ERROR: Failed to initialize BufferManager in printbufferpoll()\n");
            return ERRO_IMPRESSAO;
        }
    }

    char filepath[LEN_DB_NAME_IO * 2];
    snprintf(filepath, sizeof(filepath), "%s%s", connected.db_directory, objeto.nArquivo);

    PageResult *all_records = RM_SelectAllRecords(global_buffer_manager, filepath, s, objeto);
    if (all_records == NULL) {
        return ERRO_IMPRESSAO;
    }

    int result = printbufferpoll_adapted(all_records, s, objeto);
    return result;
}

/* -----------------------------------------------------------------------
 * excluirTuplaBuffer
 * ----------------------------------------------------------------------- */
column* excluirTuplaBuffer(tp_buffer *buffer, tp_table *campos, struct fs_objects objeto,
                            int page_id, int nTupla) {
    if (global_buffer_manager == NULL) {
        fprintf(stderr, "ERROR: BufferManager not initialized. excluirTuplaBuffer()\n");
        return ERRO_PARAMETRO;
    }

    char filepath[LEN_DB_NAME_IO * 2];
    snprintf(filepath, sizeof(filepath), "%s%s", connected.db_directory, objeto.nArquivo);

    tp_buffer* page_buffer = BM_GetPage(global_buffer_manager, filepath, (unsigned int)page_id);
    if (page_buffer == NULL) {
        fprintf(stderr, "ERROR: Page %d not found for deletion.\n", page_id);
        return ERRO_PARAMETRO;
    }

    unsigned int current_offset = 0;
    int tuple_size = tamTupla(campos, objeto);
    int found_tupla_index = 0;
    int target_offset = -1;

    while (current_offset < page_buffer->position) {
        if (page_buffer->data[current_offset] == '0') {
            if (found_tupla_index == nTupla) {
                target_offset = current_offset;
                break;
            }
            found_tupla_index++;
        }
        current_offset += tuple_size;
    }
    BM_UnpinPage(global_buffer_manager, filepath, (unsigned int)page_id);

    if (target_offset == -1) {
        fprintf(stderr, "ERROR: Tuple index %d not found in page %d.\n", nTupla, page_id);
        return ERRO_PARAMETRO;
    }

    int result = RM_DeleteRecord(global_buffer_manager, filepath, campos, objeto,
                                  (unsigned int)page_id, (unsigned int)target_offset);
    if (result != 0) {
        return ERRO_PARAMETRO;
    }
    return NULL;
}

/* -----------------------------------------------------------------------
 * getTupla
 * ----------------------------------------------------------------------- */
char* getTupla(tp_table *campos, struct fs_objects objeto, int from) {
    if (global_buffer_manager == NULL) {
        fprintf(stderr, "ERROR: BufferManager not initialized. getTupla()\n");
        return ERRO_DE_LEITURA;
    }

    char filepath[LEN_DB_NAME_IO * 2];
    snprintf(filepath, sizeof(filepath), "%s%s", connected.db_directory, objeto.nArquivo);

    unsigned int page_id = 0;
    unsigned int offset  = (unsigned int)from;

    tp_buffer* page_buffer = BM_GetPage(global_buffer_manager, filepath, page_id);
    if (page_buffer == NULL) {
        fprintf(stderr, "ERROR: Failed to get page %u for getTupla.\n", page_id);
        return ERRO_DE_LEITURA;
    }

    tupla *t = PAGE_GetTupla(page_buffer, offset, campos, objeto);
    BM_UnpinPage(global_buffer_manager, filepath, page_id);

    if (t == NULL) {
        return ERRO_DE_LEITURA;
    }

    if (t->ncols > 0 && t->column[0].valorCampo != NULL) {
        char *result_str = (char*)uffsllocType(strlen(t->column[0].valorCampo) + 1, TEMPORARY);
        if (result_str) {
            strcpy(result_str, t->column[0].valorCampo);
            return result_str;
        }
    }
    return ERRO_DE_LEITURA;
}

/* -----------------------------------------------------------------------
 * setTupla  (wrapper limitado — mantido para compatibilidade de compilação)
 * ----------------------------------------------------------------------- */
void setTupla(tp_buffer *buffer, char *tupla_data, int tam, int pos) {
    if (global_buffer_manager == NULL) {
        fprintf(stderr, "ERROR: BufferManager not initialized. setTupla()\n");
        return;
    }
    fprintf(stderr, "WARNING: setTupla wrapper is limited. Use RM_InsertRecord or PAGE_SetTupla.\n");
}

/* -----------------------------------------------------------------------
 * colocaTuplaBuffer  (wrapper limitado)
 * ----------------------------------------------------------------------- */
int colocaTuplaBuffer(tp_buffer *buffer, int from, tp_table *campos, struct fs_objects objeto) {
    if (global_buffer_manager == NULL) {
        fprintf(stderr, "ERROR: BufferManager not initialized. colocaTuplaBuffer()\n");
        return ERRO_BUFFER_CHEIO;
    }
    fprintf(stderr, "WARNING: colocaTuplaBuffer wrapper is limited. Use RM_InsertRecord.\n");
    return ERRO_BUFFER_CHEIO;
}

/* -----------------------------------------------------------------------
 * writeBufferToDisk
 *
 * Grava o frame `buffer` no disco usando o filename armazenado no próprio
 * frame. Se o filename estiver vazio (frame criado por initBuffer antes de
 * um INSERT), o objeto é usado para montar o caminho.
 * ----------------------------------------------------------------------- */
int writeBufferToDisk(tp_buffer *buffer, struct fs_objects *objeto) {
    if (global_buffer_manager == NULL) {
        fprintf(stderr, "ERROR: BufferManager not initialized. writeBufferToDisk()\n");
        return 0;
    }
    if (buffer == NULL || buffer->id == (unsigned int)INVALID_PAGE_ID) {
        fprintf(stderr, "ERROR: Invalid buffer provided to writeBufferToDisk.\n");
        return 0;
    }

    /* Determina o filepath */
    char filepath[LEN_DB_NAME_IO * 2];
    if (buffer->filename[0] != '\0') {
        strncpy(filepath, buffer->filename, sizeof(filepath) - 1);
        filepath[sizeof(filepath) - 1] = '\0';
    } else if (objeto != NULL) {
        snprintf(filepath, sizeof(filepath), "%s%s", connected.db_directory, objeto->nArquivo);
        /* Registra o filename no frame para futuros flushes */
        strncpy(buffer->filename, filepath, sizeof(buffer->filename) - 1);
        buffer->filename[sizeof(buffer->filename) - 1] = '\0';
    } else {
        fprintf(stderr, "ERROR: writeBufferToDisk: cannot determine filename.\n");
        return 0;
    }

    buffer->db = 1; /* garante que será gravado */
    if (BM_FlushPage(global_buffer_manager, filepath, buffer->id) == 0) {
        return 1;
    } else {
        return 0;
    }
}

/* Declarações externas necessárias para linkagem */
extern int cabecalho(tp_table *s, int num_reg);
extern int drawline(tupla *t, tp_table *s, struct fs_objects objeto);
