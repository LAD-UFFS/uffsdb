#pragma once

#include <stdlib.h>
#include "types.h"

void* uffsllocType(size_t size, MemoryContextType type);
void uffsFree(MemoryContextType type);
void* uffsRealloc(void *ptr, size_t newSize);
void destroyMemoryContext();

static inline void* uffslloc(size_t size) {
    return uffsllocType(size, TEMPORARY);
}