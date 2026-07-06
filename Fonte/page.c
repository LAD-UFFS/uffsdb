#include "page.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Contrato de valorCampo:
 *   - tipo 'S': string terminada em '\0', tamanho = campos[ic].tam
 *   - tipo 'I': bytes binários de int (4 bytes), tamanho = sizeof(int)
 *   - tipo 'D': bytes binários de double (8 bytes), tamanho = sizeof(double)
 *   - tipo 'C': 1 byte do char, tamanho = sizeof(char)
 *   - NULL:     valorCampo == COLUNA_NULL
 *
 * Este contrato é exigido por adcResultado() e Expressao.c (converter()).
 * drawline() em misc.c também foi corrigido para usar este contrato.
 */

/* ----------------------------------------------------------------------------------------------
    Objetivo: Extrai uma única tupla de uma posição específica dentro dos dados de um tp_buffer
    Retorno:  Retorna NULL se a tupla estiver deletada ou o offset for inválido
   ---------------------------------------------------------------------------------------------*/
tupla* PAGE_GetTupla(tp_buffer *page_buffer, unsigned int offset, tp_table *campos, struct fs_objects objeto) {
    if (!page_buffer || offset >= page_buffer->position) {
        return NULL;
    }

    char *current_ptr = page_buffer->data + offset;

    // Byte de deleção: 0 = válida, 1 = deletada 
    if ((unsigned char)current_ptr[0] != 0) {
        return NULL;
    }

    tupla *t = (tupla*)uffsllocType(sizeof(tupla), TEMPORARY);
    if (!t) {
        fprintf(stderr, "ERROR: Memory allocation failed for tupla.\n");
        return NULL;
    }

    t->offset = offset;
    t->ncols  = (unsigned int)objeto.qtdCampos;
    t->bufferPage = page_buffer->id;
    t->column = (column*)uffsllocType(sizeof(column) * objeto.qtdCampos, TEMPORARY);
    if (!t->column) {
        fprintf(stderr, "ERROR: Memory allocation failed for tupla columns.\n");
        return NULL;
    }

    current_ptr++; // avança pelo byte de deleção 

    // Lê o bitmap de nulos (1 byte por campo: 0 = não nulo, 1 = nulo) 
    unsigned char *nullos = (unsigned char*)current_ptr;
    current_ptr += objeto.qtdCampos;

    // Lê os valores dos campos 
    for (int ic = 0; ic < objeto.qtdCampos; ic++) {
        column *c = &t->column[ic];
        c->tipoCampo = campos[ic].tipo;
        strncpy(c->nomeCampo, campos[ic].nome, TAMANHO_NOME_CAMPO - 1);
        c->nomeCampo[TAMANHO_NOME_CAMPO - 1] = '\0';

        // Encadeia next para compatibilidade com drawline/misc.c 
        if (ic < objeto.qtdCampos - 1) {
            c->next = &t->column[ic + 1];
        } else {
            c->next = NULL;
        }

        if (nullos[ic] != 0) {
            // Campo nulo 
            c->valorCampo = COLUNA_NULL;
        } else {
            // Aloca e copia os bytes brutos do campo 
            c->valorCampo = (char*)uffsllocType((size_t)campos[ic].tam + 1, TEMPORARY);
            if (!c->valorCampo) {
                fprintf(stderr, "ERROR: Memory allocation failed for column value.\n");
                return NULL;
            }
            memcpy(c->valorCampo, current_ptr, (size_t)campos[ic].tam);
            // Garante terminação nula para strings 
            c->valorCampo[campos[ic].tam] = '\0';
        }
        current_ptr += campos[ic].tam;
    }

    return t;
}

// Extrai todas as tuplas válidas de um frame
PageResult* PAGE_GetTuplasFromFrame(tp_buffer *page_buffer, tp_table *campos, struct fs_objects objeto) {
    if (!page_buffer || page_buffer->position == 0) {
        return NULL;
    }

    int tuple_size = tamTupla(campos, objeto);
    if (tuple_size <= 0) return NULL;

    unsigned int max_tuplas = page_buffer->position / (unsigned int)tuple_size + 1;
    tupla **temp = (tupla**)uffsllocType(sizeof(tupla*) * max_tuplas, TEMPORARY);
    if (!temp) {
        fprintf(stderr, "ERROR: Memory allocation failed for temp tuplas array.\n");
        return NULL;
    }

    unsigned int count = 0;
    unsigned int i = 0;

    while (i + (unsigned int)tuple_size <= page_buffer->position) {
        tupla *t = PAGE_GetTupla(page_buffer, i, campos, objeto);
        if (t != NULL) {
            temp[count++] = t;
        }
        i += (unsigned int)tuple_size;
    }

    PageResult *pg = (PageResult*)uffsllocType(sizeof(PageResult), TEMPORARY);
    if (!pg) {
        fprintf(stderr, "ERROR: Memory allocation failed for PageResult.\n");
        return NULL;
    }

    pg->nrec   = (int)count;
    pg->tuplas = NULL;

    if (count > 0) {
        pg->tuplas = (tupla*)uffsllocType(sizeof(tupla) * count, TEMPORARY);
        if (!pg->tuplas) {
            fprintf(stderr, "ERROR: Memory allocation failed for PageResult tuplas.\n");
            return NULL;
        }
        for (unsigned int k = 0; k < count; k++) {
            pg->tuplas[k] = *temp[k];
        }
    }

    return pg;
}

/* ----------------------------------------------------------------------------------------------
    Objetivo: Escreve uma tupla nos dados de um tp_buffer em uma posição específica
    valorCampo deve conter os bytes binários do valor (não string para I/D/C)
    Retorno: O código de INSERT em sqlcommands.c já monta bufferTuple com bytes binários
    e depois faz memcpy para buffer->data. PAGE_SetTupla é usado pelo RM_InsertRecord
    quando se tem uma struct tupla já montada.
 -------------------*/ 
int PAGE_SetTupla(tp_buffer *page_buffer, unsigned int offset, const tupla *nova_tupla, tp_table *campos, struct fs_objects objeto) {
    if (!page_buffer || !nova_tupla || offset > SIZE) {
        fprintf(stderr, "ERROR: Invalid parameters for PAGE_SetTupla.\n");
        return -1;
    }

    int tuple_size = tamTupla(campos, objeto);
    if (tuple_size <= 0) {
        fprintf(stderr, "ERROR: Invalid tuple size in PAGE_SetTupla.\n");
        return -1;
    }

    if (offset + (unsigned int)tuple_size > SIZE) {
        fprintf(stderr, "ERROR: Tuple does not fit in page at offset %u.\n", offset);
        return -1;
    }

    char *current_ptr = page_buffer->data + offset;

    // Byte de deleção: 0 = válida 
    *current_ptr = 0;
    current_ptr++;

    // Bitmap de nulos 
    unsigned char *nullos = (unsigned char*)current_ptr;
    memset(nullos, 0, (size_t)objeto.qtdCampos);
    for (int ic = 0; ic < objeto.qtdCampos; ic++) {
        if (nova_tupla->column[ic].valorCampo == COLUNA_NULL || nova_tupla->column[ic].valorCampo == NULL) {
            nullos[ic] = 1;
        }
    }
    current_ptr += objeto.qtdCampos;

    // Valores dos campos (bytes binários) 
    for (int ic = 0; ic < objeto.qtdCampos; ic++) {
        if (nova_tupla->column[ic].valorCampo != COLUNA_NULL && nova_tupla->column[ic].valorCampo != NULL) {
            memcpy(current_ptr, nova_tupla->column[ic].valorCampo, (size_t)campos[ic].tam);
        } else {
            memset(current_ptr, 0, (size_t)campos[ic].tam);
        }
        current_ptr += campos[ic].tam;
    }

    // Atualiza metadados do frame 
    unsigned int new_end = offset + (unsigned int)tuple_size;
    if (new_end > page_buffer->position) {
        page_buffer->position = new_end;
    }
    page_buffer->nrec++;
    page_buffer->db = 1;

    return 0;
}
