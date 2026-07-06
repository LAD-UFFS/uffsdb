#include "record_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FMISC
  #include "misc.h"
#endif

/**
 * @brief Imprime os registros de um PageResult.
 */
int printbufferpoll_adapted(PageResult *pr, tp_table *s, struct fs_objects objeto) {
    if (pr == NULL || pr->nrec == 0) {
        return ERRO_IMPRESSAO;
    }
    cabecalho(s, objeto.qtdCampos);
    for (unsigned int i = 0; i < (unsigned int)pr->nrec; i++) {
        drawline(&pr->tuplas[i], s, objeto);
    }
    return SUCCESS;
}

/**
 * @brief Stub de addColumn (mantido para compatibilidade de linkagem).
 */
void addColumn(column **colList, column *c) {
    (void)colList;
    (void)c;
}

/* -----------------------------------------------------------------------
 * RM_InsertRecord
 *
 * Insere uma nova tupla na tabela identificada por `filename`.
 * Percorre as páginas existentes procurando espaço. Se não houver,
 * cria uma nova página (o arquivo é expandido automaticamente pelo
 * Disk_WritePage quando a página é descarregada).
 * ----------------------------------------------------------------------- */
int RM_InsertRecord(BufferManager* bm, const char* filename,
                    tp_table *s, struct fs_objects objeto,
                    const tupla *nova_tupla) {
    if (!bm || !s || !nova_tupla || !filename) {
        fprintf(stderr, "ERROR: Invalid parameters for RM_InsertRecord.\n");
        return -1;
    }

    int tuple_size = tamTupla(s, objeto);
    if (tuple_size <= 0 || (unsigned int)tuple_size > bm->pool->page_size) {
        fprintf(stderr, "ERROR: Tuple size (%d) exceeds page size (%u).\n",
                tuple_size, bm->pool->page_size);
        return -1;
    }

    /* Tenta encontrar uma página existente com espaço suficiente */
    for (unsigned int page_id = 0; page_id < PAGES; page_id++) {
        tp_buffer* page_buffer = BM_GetPage(bm, filename, page_id);
        if (page_buffer == NULL) {
            /* Não conseguiu carregar a página — tenta criar uma nova */
            break;
        }

        if (page_buffer->position + (unsigned int)tuple_size <= bm->pool->page_size) {
            /* Há espaço: insere a tupla */
            if (PAGE_SetTupla(page_buffer, page_buffer->position, nova_tupla, s, objeto) != 0) {
                BM_UnpinPage(bm, filename, page_id);
                return -1;
            }
            BM_MarkDirty(bm, filename, page_id);
            BM_UnpinPage(bm, filename, page_id);
            return 0;
        }
        BM_UnpinPage(bm, filename, page_id);
    }

    /*
     * Nenhuma página existente tem espaço suficiente.
     * Cria uma nova página: BM_GetPage carregará uma página vazia (zeros)
     * porque Disk_ReadPage trata arquivos menores que o offset como página nova.
     */
    for (unsigned int page_id = 0; page_id < PAGES; page_id++) {
        tp_buffer* page_buffer = BM_GetPage(bm, filename, page_id);
        if (page_buffer == NULL) continue;

        /* Página nova tem position == 0 */
        if (page_buffer->position == 0) {
            if (PAGE_SetTupla(page_buffer, 0, nova_tupla, s, objeto) != 0) {
                BM_UnpinPage(bm, filename, page_id);
                return -1;
            }
            BM_MarkDirty(bm, filename, page_id);
            BM_UnpinPage(bm, filename, page_id);
            return 0;
        }
        BM_UnpinPage(bm, filename, page_id);
    }

    fprintf(stderr, "ERROR: No space found to insert record in %s.\n", filename);
    return -1;
}

/* -----------------------------------------------------------------------
 * RM_DeleteRecord
 * ----------------------------------------------------------------------- */
int RM_DeleteRecord(BufferManager* bm, const char* filename,
                    tp_table *s, struct fs_objects objeto,
                    unsigned int page_id, unsigned int offset) {
    if (!bm || !s || !filename || page_id >= PAGES) {
        fprintf(stderr, "ERROR: Invalid parameters for RM_DeleteRecord.\n");
        return -1;
    }

    tp_buffer* page_buffer = BM_GetPage(bm, filename, page_id);
    if (page_buffer == NULL) {
        fprintf(stderr, "ERROR: Page %u not found for deletion.\n", page_id);
        return -1;
    }

    int tuple_size = tamTupla(s, objeto);
    if (offset + (unsigned int)tuple_size > page_buffer->position) {
        fprintf(stderr, "ERROR: Invalid offset %u for page %u.\n", offset, page_id);
        BM_UnpinPage(bm, filename, page_id);
        return -1;
    }

    page_buffer->data[offset] = 1; /* marca como deletada (byte de deleção = 1) */
    if (page_buffer->nrec > 0) page_buffer->nrec--;
    BM_MarkDirty(bm, filename, page_id);
    BM_UnpinPage(bm, filename, page_id);

    return 0;
}

/* -----------------------------------------------------------------------
 * RM_UpdateRecord
 * ----------------------------------------------------------------------- */
int RM_UpdateRecord(BufferManager* bm, const char* filename,
                    tp_table *s, struct fs_objects objeto,
                    unsigned int page_id, unsigned int offset,
                    const tupla *tupla_atualizada) {
    if (!bm || !s || !tupla_atualizada || !filename || page_id >= PAGES) {
        fprintf(stderr, "ERROR: Invalid parameters for RM_UpdateRecord.\n");
        return -1;
    }

    tp_buffer* page_buffer = BM_GetPage(bm, filename, page_id);
    if (page_buffer == NULL) {
        fprintf(stderr, "ERROR: Page %u not found for update.\n", page_id);
        return -1;
    }

    int tuple_size = tamTupla(s, objeto);
    if (offset + (unsigned int)tuple_size > page_buffer->position) {
        fprintf(stderr, "ERROR: Invalid offset %u for page %u.\n", offset, page_id);
        BM_UnpinPage(bm, filename, page_id);
        return -1;
    }

    if (PAGE_SetTupla(page_buffer, offset, tupla_atualizada, s, objeto) != 0) {
        BM_UnpinPage(bm, filename, page_id);
        return -1;
    }

    BM_MarkDirty(bm, filename, page_id);
    BM_UnpinPage(bm, filename, page_id);
    return 0;
}

/* -----------------------------------------------------------------------
 * RM_SelectAllRecords
 *
 * Itera pelas páginas do arquivo `filename` e coleta todas as tuplas válidas.
 * Para quando encontra uma página vazia (position == 0) ou quando BM_GetPage
 * falha (fim lógico do arquivo).
 * ----------------------------------------------------------------------- */
PageResult* RM_SelectAllRecords(BufferManager* bm, const char* filename,
                                 tp_table *s, struct fs_objects objeto) {
    if (!bm || !s || !filename) {
        fprintf(stderr, "ERROR: Invalid parameters for RM_SelectAllRecords.\n");
        return NULL;
    }

    int tuple_size = tamTupla(s, objeto);
    if (tuple_size <= 0) return NULL;

    unsigned int max_per_page = bm->pool->page_size / (unsigned int)tuple_size + 1;
    tupla **all_temp = (tupla**)uffsllocType(sizeof(tupla*) * PAGES * max_per_page, TEMPORARY);
    if (!all_temp) {
        fprintf(stderr, "ERROR: Memory allocation failed for RM_SelectAllRecords.\n");
        return NULL;
    }
    unsigned int total = 0;

    for (unsigned int page_id = 0; page_id < PAGES; page_id++) {
        tp_buffer* page_buffer = BM_GetPage(bm, filename, page_id);
        if (page_buffer == NULL) break;

        /* Página vazia = fim lógico do arquivo */
        if (page_buffer->position == 0) {
            BM_UnpinPage(bm, filename, page_id);
            break;
        }

        PageResult *pr = PAGE_GetTuplasFromFrame(page_buffer, s, objeto);
        if (pr != NULL) {
            for (int i = 0; i < pr->nrec; i++) {
                tupla *cp = (tupla*)uffsllocType(sizeof(tupla), TEMPORARY);
                if (!cp) { BM_UnpinPage(bm, filename, page_id); goto done; }
                memcpy(cp, &pr->tuplas[i], sizeof(tupla));

                cp->column = (column*)uffsllocType(sizeof(column) * cp->ncols, TEMPORARY);
                if (!cp->column) { BM_UnpinPage(bm, filename, page_id); goto done; }

                for (unsigned int ci = 0; ci < cp->ncols; ci++) {
                    memcpy(&cp->column[ci], &pr->tuplas[i].column[ci], sizeof(column));
                    if (pr->tuplas[i].column[ci].valorCampo != COLUNA_NULL &&
                        pr->tuplas[i].column[ci].valorCampo != NULL) {
                        size_t vlen = strlen(pr->tuplas[i].column[ci].valorCampo) + 1;
                        cp->column[ci].valorCampo = (char*)uffsllocType(vlen, TEMPORARY);
                        if (cp->column[ci].valorCampo)
                            strcpy(cp->column[ci].valorCampo, pr->tuplas[i].column[ci].valorCampo);
                    } else {
                        cp->column[ci].valorCampo = COLUNA_NULL;
                    }
                }
                all_temp[total++] = cp;
            }
        }
        BM_UnpinPage(bm, filename, page_id);
    }

done:;
    PageResult *result = (PageResult*)uffsllocType(sizeof(PageResult), TEMPORARY);
    if (!result) return NULL;

    result->nrec   = (int)total;
    result->tuplas = NULL;

    if (total > 0) {
        result->tuplas = (tupla*)uffsllocType(sizeof(tupla) * total, TEMPORARY);
        if (!result->tuplas) return NULL;
        for (unsigned int i = 0; i < total; i++) {
            result->tuplas[i] = *all_temp[i];
        }
    }

    return result;
}
