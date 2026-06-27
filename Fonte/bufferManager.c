#include <stdio.h>
#include <stdlib.h>

#ifndef FMACROS
    #include "macros.h"
#endif

#ifndef FTYPES
    #include "types.h"
#endif

#include "buffer.h"
#include "bufferManager.h"

// intermediário para o getBlock (testei os comandos (insert, delete...) e funcionou normalmente. As impressões que dizem se o bloco está ou não no buffer parecem funcionar. Mas não criei uma função de imprimir o buffer pra conferir o buffer)
// verifica se o bloco id_bloco da tabela id_tabela já está no buffer. Se estiver, retorna um ponteiro para ele. Se não estiver, lê o bloco do disco com getBlock, copia para o primeiro slot livre do buffer, atualiza o header dele e retorna um ponteiro para esse slot
// daí agora todo lugar que chama getBlock() tem que passar a chamar bm_getBlock, imagino eu
tp_pagina *bm_getBlock(int id_tabela, int id_bloco, char *filename) {

    // vendo se o bloco (id_bloco) da tabela (id_tabela) já está no buffer pool:
    for (int i = 0; i < bp.qtd_paginas_total; i++) {
        if (bp.header[i].id_tabela == id_tabela &&  bp.header[i].bloco_da_tabela == id_bloco) {
            printf("bm_getBlock: bloco %d da tabela com id %d e filename %s JÁ ESTÁ no buffer\n", id_bloco, id_tabela, filename);
            return &bp.paginas[i]; // página já está no buffer
        }
    }

    // se a página não estiver no buffer, trazemos ela do disco e colocamos no buffer
    
    // mas antes precisamos ver se tem espaço no buffer pra colocar a página:
    int indice_disponivel = -1; // slot disponível no buffer pool
    for (int i = 0; i < bp.qtd_paginas_total; i++) {
        if (bp.header[i].id_tabela == -1) { // -1 = livre
            indice_disponivel = i;
            break;
        }
    }

    // se o buffer tiver cheio, o professor disse que podíamos dar panic (que não precisávamos implementar uma política de trocas):
    if (indice_disponivel == -1) {
        printf("ERROR: buffer pool cheio\n");
        exit(1);
    }

    printf("bm_getBlock: bloco %d da tabela com id %d e filename %s NÃO ESTÁ no buffer e será lido do disco\n", id_bloco, id_tabela, filename);
    tp_pagina *bloco = getBlock((unsigned int)id_bloco, filename);

    // copiando o conteúdo do bloco para o slot livre que achamos do buffer pool (copiando o conteúdo mesmo (por isso "*bloco")
    bp.paginas[indice_disponivel] = *bloco;

    // colocando no header do buffer que o slot "indice_disponivel" agora está ocupado pelo bloco "id_bloco" da tabela "id_tabela"
    bp.header[indice_disponivel].id_tabela = id_tabela;
    bp.header[indice_disponivel].bloco_da_tabela = id_bloco;
    bp.header[indice_disponivel].db = 0;
    bp.header[indice_disponivel].pc = 1;
    bp.qtd_paginas_ocupadas++;
    bp.qtd_paginas_desocupadas--;

    return &bp.paginas[indice_disponivel];
}


// como getPage chama getBlock dentro dela, não tem como fazer o bm_getPage da mesma forma que eu fiz o bm_getBlock (chamei getBlock dentro de bm_getBlock para ele fazer o trabalho dele, sem alterar getBlock), pq se bm_getPage chamasse getPage, o getPage leria do disco toda vez (pq ele chama getBlock), e daí o buffer nem seria usado
// estava pensando como iria fazer esse intermediário para o getPage (bm_getPage), e daí pensei que eu poderia não fazer ele, e simplesmente mudar o getPage mesmo, para ele chamar bm_getBlock em vez de getBlock, pq daí ao invés de buscar no disco toda vez, ele chama bm_getBlock, que já verifica se o bloco tá no buffer, e esse é o objetivo (usar o buffer ao invés de toda vez buscar direto no disco)
// PageResult *bm_getPage(/*...*/) {

// }


void bm_printHeaderBufferPool()
{
    printf("\n------ Header Buffer Pool ------\n");
    printf("total: %d | ocupados: %d | livres: %d\n\n", bp.qtd_paginas_total, bp.qtd_paginas_ocupadas, bp.qtd_paginas_desocupadas);
    printf("slot       id_tabela  bloco    dp      pc\n");
    for (int i = 0; i < bp.qtd_paginas_total; i++)
    {
        if (bp.header[i].id_tabela == -1)
        {
            continue; // pulando slots livres
        }
        printf("%d          %d          %d        %d       %d\n", i, bp.header[i].id_tabela, bp.header[i].bloco_da_tabela, bp.header[i].db, bp.header[i].pc);
    }
    printf("---------------------------------\n");
}