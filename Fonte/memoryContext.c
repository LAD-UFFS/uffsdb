#include "memoryContext.h"
#include "types.h" // MemoryContextType e MemoryContextRoot
#include "macros.h"
#include <stdio.h> // NULL, printf
#include <string.h> // memcpy
#include <stdlib.h> // calloc, free

MemoryContextRoot root = {0};
static void uffsFreeRecursive(MemoryContext *context);

void* uffsllocType(size_t size, MemoryContextType type) {
    //  Para o futuro seria interessante criar uma pagina para allocações < 8 bytes, 
    //      pois a struct de metadados tem 8 bytes ,
    //      o que acaba usando muito espaço, duas alocações de 1 byte usam 18.
    size += 1;
    MemoryContext *context = type == TEMPORARY ? root.temporary : root.permanent;
    while(context) {
        // confere se na lista tem algum filho com um espacinho que não foi usado antes
        if(context->used + size <= MEMORY_CONTEXT_SIZE) { 
            uffs_mem_header *ptr = (uffs_mem_header *)(context->memoryPool + context->used);
            ptr->size = size;
            context->used += sizeof(uffs_mem_header) + size;
            ptr->data[size - 1] = '\0';
            return ptr->data;
        }
        context = context->next;
    }

    // se não tem espaço em nenhum contexto, cria um novo
    MemoryContext *newContext = calloc(1, sizeof(MemoryContext));
    if(!newContext) {
        printf("Memory allocation failed for new memory context\n");
        return NULL;
    }

    newContext->type = type;
    newContext->used =  sizeof(uffs_mem_header) + size;

    if(type == TEMPORARY) {
        newContext->next = root.temporary ? root.temporary->next : root.temporary;
        root.temporary = newContext;
    } else {
        newContext->next = root.permanent ? root.permanent->next : root.permanent;
        root.permanent = newContext;
    }

    uffs_mem_header *ptr = (uffs_mem_header *)(newContext->memoryPool);
    ptr->size = size;
    ptr->data[size - 1] = '\0';
    return ptr->data;
}

// atualmente faz só do temporario, que é o necessario
void uffsFree(MemoryContextType type) {
    if (!root.temporary) return;
    MemoryContext *context = root.temporary;
    uffsFreeRecursive(context->next);
    memset(context, 0, sizeof(MemoryContext));
}

void uffsFreeRecursive(MemoryContext *context){
    if(context == NULL) return;
    uffsFreeRecursive(context->next);
    free(context);
    context = NULL;
}

void *uffsRealloc(void *ptr, size_t newSize) {
    if(ptr == NULL) return uffslloc(newSize);

    uffs_mem_header *header = (uffs_mem_header *)((char *)ptr - sizeof(uffs_mem_header));
    size_t oldSize = header->size - 1; // desconsidera o \0
    if(newSize <= oldSize) {
        header->size = newSize + 1;
        header->data[newSize] = '\0';
        return ptr;
    }
    void *newPtr = uffslloc(newSize);

    memcpy(newPtr, ptr, oldSize); 
    return newPtr;
}

void destroyMemoryContext() {
    if (root.temporary) {
        uffsFreeRecursive(root.temporary);
        root.temporary = NULL;
    }
    if (root.permanent) {
        uffsFreeRecursive(root.permanent);
        root.permanent = NULL;
    }
}