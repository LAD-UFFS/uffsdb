#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H

#include <stdio.h>

#ifndef FTYPES // garante que types.h não seja reincluída
  #include "types.h"
#endif

// Funções para gerenciamento de disco
FILE* Disk_Open(const char* filename);
void Disk_Close(FILE* fd);
int Disk_ReadPage(FILE* fd, unsigned int page_id, tp_buffer* page_buffer, unsigned int page_size);
int Disk_WritePage(FILE* fd, unsigned int page_id, const tp_buffer* page_buffer, unsigned int page_size);

// Conveniências: abrem, operam e fecham o arquivo em uma única chamada 
int Disk_ReadPageByName(const char* filename, unsigned int page_id, tp_buffer* page_buffer, unsigned int page_size);
int Disk_WritePageByName(const char* filename, unsigned int page_id, const tp_buffer* page_buffer, unsigned int page_size);

#endif // DISK_MANAGER_H
