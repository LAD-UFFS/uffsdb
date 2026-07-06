#include "buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * INVALID_PAGE_ID é definido como -23 em macros.h.
 * Como tp_buffer.id é unsigned int, o valor -23 em complemento de dois
 * equivale a 0xFFFFFFE9 (4294967273). Usamos esse mesmo valor como sentinela.
 */

/**
 * @brief Inicializa o Buffer Pool com a quantidade de frames e tamanho de página especificados.
 */
BufferPool* BP_Init(unsigned int num_frames, unsigned int page_size) {
    /* Usa malloc diretamente: o pool é grande demais para o MemoryContext (16KB por bloco) */
    BufferPool* pool = (BufferPool*)calloc(1, sizeof(BufferPool));
    if (pool == NULL) {
        fprintf(stderr, "ERROR: Memory allocation failed for BufferPool.\n");
        return NULL;
    }

    pool->num_frames = num_frames;
    pool->page_size  = page_size;
    pool->clock      = 0;

    pool->frames = (tp_buffer*)calloc(num_frames, sizeof(tp_buffer));
    if (pool->frames == NULL) {
        fprintf(stderr, "ERROR: Memory allocation failed for BufferPool frames.\n");
        free(pool);
        return NULL;
    }

    /* Inicializa cada frame */
    for (unsigned int i = 0; i < num_frames; i++) {
        pool->frames[i].id          = (unsigned int)INVALID_PAGE_ID;
        pool->frames[i].db          = 0;
        pool->frames[i].pc          = 0;
        pool->frames[i].last_access = 0;
        pool->frames[i].nrec        = 0;
        pool->frames[i].position    = 0;
        memset(pool->frames[i].data, 0, page_size);
        /* Cada frame guarda o nome do arquivo de onde foi carregado */
        pool->frames[i].filename[0] = '\0';
    }

    return pool;
}

/**
 * @brief Destrói o Buffer Pool.
 *        Não realoca memória — apenas sinaliza que o pool não é mais válido.
 *        A liberação real é feita por destroyMemoryContext() ou uffsFree(PERMANENT).
 */
void BP_Destroy(BufferPool* pool) {
    if (pool == NULL) {
        return;
    }
    /* Invalida todos os frames para evitar uso acidental após destroy */
    for (unsigned int i = 0; i < pool->num_frames; i++) {
        pool->frames[i].id = (unsigned int)INVALID_PAGE_ID;
        pool->frames[i].db = 0;
        pool->frames[i].pc = 0;
    }
    free(pool->frames);
    free(pool);
}

/**
 * @brief Procura uma página específica no Buffer Pool pelo par (page_id, filename).
 *        Isso é necessário porque o pool pode conter páginas de arquivos diferentes.
 */
tp_buffer* BP_FindPage(BufferPool* pool, unsigned int page_id) {
    if (pool == NULL) {
        return NULL;
    }
    for (unsigned int i = 0; i < pool->num_frames; i++) {
        if (pool->frames[i].id != (unsigned int)INVALID_PAGE_ID &&
            pool->frames[i].id == page_id) {
            return &pool->frames[i];
        }
    }
    return NULL;
}

/**
 * @brief Procura uma página pelo par (page_id, filename).
 *        Versão estendida usada pelo BM quando há múltiplos arquivos.
 */
tp_buffer* BP_FindPageByFile(BufferPool* pool, unsigned int page_id, const char* filename) {
    if (pool == NULL || filename == NULL) {
        return NULL;
    }
    for (unsigned int i = 0; i < pool->num_frames; i++) {
        if (pool->frames[i].id != (unsigned int)INVALID_PAGE_ID &&
            pool->frames[i].id == page_id &&
            strncmp(pool->frames[i].filename, filename, sizeof(pool->frames[i].filename)) == 0) {
            return &pool->frames[i];
        }
    }
    return NULL;
}

/**
 * @brief Encontra um frame livre (id == INVALID_PAGE_ID) no Buffer Pool.
 */
tp_buffer* BP_GetFreeFrame(BufferPool* pool) {
    if (pool == NULL) {
        return NULL;
    }
    for (unsigned int i = 0; i < pool->num_frames; i++) {
        if (pool->frames[i].id == (unsigned int)INVALID_PAGE_ID) {
            return &pool->frames[i];
        }
    }
    return NULL;
}

/**
 * @brief Seleciona um frame vítima para substituição usando LRU.
 *        Páginas com pc > 0 não podem ser substituídas.
 */
tp_buffer* BP_SelectVictim(BufferPool* pool) {
    if (pool == NULL) {
        return NULL;
    }

    tp_buffer* victim = NULL;
    long min_access_time = -1;

    for (unsigned int i = 0; i < pool->num_frames; i++) {
        if (pool->frames[i].pc > 0) {
            continue;
        }
        if (pool->frames[i].id == (unsigned int)INVALID_PAGE_ID) {
            return &pool->frames[i];
        }
        if (victim == NULL || pool->frames[i].last_access < min_access_time) {
            victim = &pool->frames[i];
            min_access_time = pool->frames[i].last_access;
        }
    }
    return victim;
}

/**
 * @brief Atualiza o tempo de último acesso de um frame (para LRU).
 */
void BP_UpdateAccess(BufferPool* pool, tp_buffer* frame) {
    if (pool != NULL && frame != NULL) {
        frame->last_access = ++pool->clock;
    }
}
