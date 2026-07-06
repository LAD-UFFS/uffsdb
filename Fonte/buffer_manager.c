#include "buffer_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Inicializa o Buffer Manager e o Buffer Pool.
 *        Não abre nenhum arquivo — o arquivo é especificado em cada operação.
 */
BufferManager* BM_Init(unsigned int num_frames, unsigned int page_size) {
    BufferManager* bm = (BufferManager*)calloc(1, sizeof(BufferManager));
    if (bm == NULL) {
        fprintf(stderr, "ERROR: Memory allocation failed for BufferManager.\n");
        return NULL;
    }

    bm->pool = BP_Init(num_frames, page_size);
    if (bm->pool == NULL) {
        fprintf(stderr, "ERROR: Failed to initialize Buffer Pool.\n");
        return NULL;
    }

    bm->num_reads  = 0;
    bm->num_writes = 0;

    return bm;
}

/**
 * @brief Destrói o Buffer Manager, descarregando todas as páginas sujas.
 */
void BM_Destroy(BufferManager* bm) {
    if (bm == NULL) {
        return;
    }
    BM_FlushAll(bm);
    BP_Destroy(bm->pool);
    free(bm);
}

/**
 * @brief Carrega uma página do disco para um frame específico.
 *        Se o frame já contiver uma página suja, ela é gravada antes de ser substituída.
 */
int BM_LoadPage(BufferManager* bm, const char* filename, unsigned int page_id, tp_buffer* target_frame) {
    if (bm == NULL || target_frame == NULL || filename == NULL) {
        fprintf(stderr, "ERROR: Invalid parameters for BM_LoadPage.\n");
        return -1;
    }

    /* Se o frame está ocupado e sujo, grava antes de substituir */
    if (target_frame->id != (unsigned int)INVALID_PAGE_ID && target_frame->db) {
        if (Disk_WritePageByName(target_frame->filename, target_frame->id,
                                  target_frame, bm->pool->page_size) != 0) {
            fprintf(stderr, "ERROR: Failed to flush dirty victim page %u (%s).\n",
                    target_frame->id, target_frame->filename);
            return -1;
        }
        bm->num_writes++;
    }

    /* Carrega a nova página do disco (ou inicializa com zeros se for nova) */
    if (Disk_ReadPageByName(filename, page_id, target_frame, bm->pool->page_size) != 0) {
        fprintf(stderr, "ERROR: Failed to read page %u from disk (%s).\n", page_id, filename);
        return -1;
    }
    bm->num_reads++;

    /* Atualiza metadados do frame */
    target_frame->id = page_id;
    target_frame->db = 0;
    target_frame->pc = 1; /* pinado automaticamente ao ser carregado */
    strncpy(target_frame->filename, filename, sizeof(target_frame->filename) - 1);
    target_frame->filename[sizeof(target_frame->filename) - 1] = '\0';
    BP_UpdateAccess(bm->pool, target_frame);

    return 0;
}

/**
 * @brief Obtém uma página do Buffer Pool pelo par (filename, page_id).
 *        Se não estiver em memória, carrega do disco.
 */
tp_buffer* BM_GetPage(BufferManager* bm, const char* filename, unsigned int page_id) {
    if (bm == NULL || filename == NULL) {
        return NULL;
    }

    /* 1. Procura no pool pelo par (filename, page_id) */
    tp_buffer* frame = BP_FindPageByFile(bm->pool, page_id, filename);

    if (frame != NULL) {
        /* Cache Hit */
        frame->pc++;
        BP_UpdateAccess(bm->pool, frame);
        return frame;
    }

    /* Cache Miss: precisa carregar do disco */

    /* 2. Tenta um frame livre */
    frame = BP_GetFreeFrame(bm->pool);

    if (frame == NULL) {
        /* 3. Seleciona vítima LRU */
        frame = BP_SelectVictim(bm->pool);
        if (frame == NULL) {
            fprintf(stderr, "ERROR: No available frames (all pinned?).\n");
            return NULL;
        }
    }

    if (BM_LoadPage(bm, filename, page_id, frame) != 0) {
        return NULL;
    }

    return frame;
}

/**
 * @brief Marca uma página como suja (dirty).
 */
void BM_MarkDirty(BufferManager* bm, const char* filename, unsigned int page_id) {
    if (bm == NULL || filename == NULL) return;
    tp_buffer* frame = BP_FindPageByFile(bm->pool, page_id, filename);
    if (frame != NULL) {
        frame->db = 1;
    } else {
        fprintf(stderr, "WARNING: Attempted to mark non-existent page %u (%s) as dirty.\n",
                page_id, filename);
    }
}

/**
 * @brief Incrementa o pin_count de uma página.
 */
void BM_PinPage(BufferManager* bm, const char* filename, unsigned int page_id) {
    if (bm == NULL || filename == NULL) return;
    tp_buffer* frame = BP_FindPageByFile(bm->pool, page_id, filename);
    if (frame != NULL) {
        frame->pc++;
    } else {
        fprintf(stderr, "WARNING: Attempted to pin non-existent page %u (%s).\n",
                page_id, filename);
    }
}

/**
 * @brief Decrementa o pin_count de uma página.
 */
void BM_UnpinPage(BufferManager* bm, const char* filename, unsigned int page_id) {
    if (bm == NULL || filename == NULL) return;
    tp_buffer* frame = BP_FindPageByFile(bm->pool, page_id, filename);
    if (frame != NULL) {
        if (frame->pc > 0) {
            frame->pc--;
        } else {
            fprintf(stderr, "WARNING: Attempted to unpin page %u (%s) with pin_count already 0.\n",
                    page_id, filename);
        }
    } else {
        fprintf(stderr, "WARNING: Attempted to unpin non-existent page %u (%s).\n",
                page_id, filename);
    }
}

/**
 * @brief Grava uma página suja específica no disco.
 */
int BM_FlushPage(BufferManager* bm, const char* filename, unsigned int page_id) {
    if (bm == NULL || filename == NULL) return -1;
    tp_buffer* frame = BP_FindPageByFile(bm->pool, page_id, filename);
    if (frame != NULL) {
        if (frame->db) {
            if (Disk_WritePageByName(frame->filename, frame->id,
                                      frame, bm->pool->page_size) != 0) {
                fprintf(stderr, "ERROR: Failed to write page %u (%s) to disk during flush.\n",
                        page_id, filename);
                return -1;
            }
            bm->num_writes++;
            frame->db = 0;
        }
        return 0;
    } else {
        fprintf(stderr, "WARNING: Attempted to flush non-existent page %u (%s).\n",
                page_id, filename);
        return -1;
    }
}

/**
 * @brief Grava todas as páginas sujas do pool no disco.
 */
void BM_FlushAll(BufferManager* bm) {
    if (bm == NULL) return;
    for (unsigned int i = 0; i < bm->pool->num_frames; i++) {
        tp_buffer* frame = &bm->pool->frames[i];
        if (frame->id != (unsigned int)INVALID_PAGE_ID && frame->db) {
            if (Disk_WritePageByName(frame->filename, frame->id,
                                      frame, bm->pool->page_size) != 0) {
                fprintf(stderr, "ERROR: Failed to write page %u (%s) during BM_FlushAll.\n",
                        frame->id, frame->filename);
            } else {
                bm->num_writes++;
                frame->db = 0;
            }
        }
    }
}
