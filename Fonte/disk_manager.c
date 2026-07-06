#include "disk_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* ----------------------------------------------------------------------------------------------
    Objetivo:   Abre um arquivo no modo de leitura e escrita binária
    Retorno:     Se o arquivo não existir, ele é criado.
   ---------------------------------------------------------------------------------------------*/
FILE* Disk_Open(const char* filename) {
    FILE* fd = fopen(filename, "r+b");
    if (!fd) {
        fd = fopen(filename, "w+b");
        if (!fd) {
            fprintf(stderr, "ERROR: Failed to open or create file %s\n", filename);
            return NULL;
        }
    }
    return fd;
}

// Fecha um arquivo.
void Disk_Close(FILE* fd) {
    if (fd) {
        fclose(fd);
    }
}


/* ----------------------------------------------------------------------------------------------
    Objetivo:  Lê uma página específica do disco para o campo data de um tp_buffer.
    Retorno:   Se o arquivo for menor que o offset esperado (página nova/vazia), preenche
    data com zeros e retorna 0 (sucesso), não é erro ter uma página nova.
   ---------------------------------------------------------------------------------------------*/
int Disk_ReadPage(FILE* fd, unsigned int page_id, tp_buffer* page_buffer, unsigned int page_size) {
    if (!fd || !page_buffer) {
        fprintf(stderr, "ERROR: Invalid file descriptor or page_buffer.\n");
        return -1;
    }

    long int offset = (long int)page_id * (long int)page_size;

    //  Verifica o tamanho atual do arquivo 
    if (fseek(fd, 0, SEEK_END) != 0) {
        fprintf(stderr, "ERROR: Failed to seek to end of file.\n");
        return -1;
    }
    long int file_size = ftell(fd);

    if (file_size < 0) {
        fprintf(stderr, "ERROR: Failed to get file size.\n");
        return -1;
    }

    //  Se a página ainda não existe no arquivo, inicializa com zeros 
    if (offset >= file_size) {
        memset(page_buffer->data, 0, page_size);
        return 0;
    }

    //  Posiciona no início da página 
    if (fseek(fd, offset, SEEK_SET) != 0) {
        fprintf(stderr, "ERROR: Failed to seek to page %u in file.\n", page_id);
        return -1;
    }

    size_t bytes_read = fread(page_buffer->data, 1, page_size, fd);
    if (bytes_read < page_size) {
        // Página parcialmente escrita: preenche o restante com zeros 
        memset(page_buffer->data + bytes_read, 0, page_size - bytes_read);
    }

    return 0;
}

/* ----------------------------------------------------------------------------------------------
    Objetivo:  Escreve o campo data de um tp_buffer em uma página específica do disco
    Retorno:  Se o arquivo for menor que o offset necessário, preenche o espaço
    intermediário com zeros antes de escrever
   ---------------------------------------------------------------------------------------------*/
 int Disk_WritePage(FILE* fd, unsigned int page_id, const tp_buffer* page_buffer, unsigned int page_size) {
    if (!fd || !page_buffer) {
        fprintf(stderr, "ERROR: Invalid file descriptor or page_buffer.\n");
        return -1;
    }

    long int offset = (long int)page_id * (long int)page_size;

    // Verifica o tamanho atual do arquivo 
    if (fseek(fd, 0, SEEK_END) != 0) {
        fprintf(stderr, "ERROR: Failed to seek to end of file for write.\n");
        return -1;
    }
    long int file_size = ftell(fd);

    // Se há lacuna entre o fim do arquivo e o offset desejado, preenche com zeros 
    if (file_size < offset) {
        long int gap = offset - file_size;
        char *zeros = calloc(1, gap);
        if (!zeros) {
            fprintf(stderr, "ERROR: Memory allocation failed for gap fill.\n");
            return -1;
        }
        if (fwrite(zeros, 1, gap, fd) != (size_t)gap) {
            fprintf(stderr, "ERROR: Failed to fill gap before page %u.\n", page_id);
            free(zeros);
            return -1;
        }
        free(zeros);
    }

    // Posiciona no início da página 
    if (fseek(fd, offset, SEEK_SET) != 0) {
        fprintf(stderr, "ERROR: Failed to seek to page %u in file.\n", page_id);
        return -1;
    }

    if (fwrite(page_buffer->data, 1, page_size, fd) != page_size) {
        fprintf(stderr, "ERROR: Failed to write page %u to file.\n", page_id);
        return -1;
    }

    fflush(fd);
    return 0;
}

// abre o arquivo por nome, lê a página e fecha o arquivo
int Disk_ReadPageByName(const char* filename, unsigned int page_id, tp_buffer* page_buffer, unsigned int page_size) {
    FILE* fd = Disk_Open(filename);
    if (!fd) return -1;
    int result = Disk_ReadPage(fd, page_id, page_buffer, page_size);
    Disk_Close(fd);
    return result;
}

// abre o arquivo por nome, escreve a página e fecha o arquivo
int Disk_WritePageByName(const char* filename, unsigned int page_id, const tp_buffer* page_buffer, unsigned int page_size) {
    FILE* fd = Disk_Open(filename);
    if (!fd) return -1;
    int result = Disk_WritePage(fd, page_id, page_buffer, page_size);
    Disk_Close(fd);
    return result;
}
