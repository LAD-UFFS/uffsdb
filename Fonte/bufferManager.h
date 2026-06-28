#define FBUFFERMANAGER 1

#ifndef FMACROS
    #include "macros.h"
#endif

#ifndef FTYPES
    #include "types.h"
#endif

// verifica se o bloco id_bloco da tabela id_tabela já tá no buffer
    // - se sim, retorna um ponteiro pra ele
    // - se não, lê o bloco do disco com getBlock, copia pro buffer e retorna um ponteiro pra página do buffer que acabou de ser preenchida com o bloco lido do disco
tp_pagina *bm_getBlock(int id_tabela, int id_bloco, char *filename);

// imprime o header do buffer
void bm_printHeaderBufferPool();

// cria uma página nova diretamente no buffer, com db=1
tp_pagina *bm_novaPaginaNoBuffer(int id_tabela, int id_bloco, char *filename);

void bm_marcarDirtyBit(tp_pagina *pagina);