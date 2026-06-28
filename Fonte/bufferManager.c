#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FMACROS
#include "macros.h"
#endif

#ifndef FTYPES
#include "types.h"
#endif

#ifndef FDICTIONARY
#include "dictionary.h"
#endif

#include "buffer.h"
#include "bufferManager.h"

int pagina_da_vez_para_sair = 0;

// intermediário para o getBlock (testei os comandos (insert, delete...) e funcionou normalmente. As impressões que dizem se o bloco está ou não no buffer parecem funcionar. Mas não criei uma função de imprimir o buffer pra conferir o buffer)
// verifica se o bloco id_bloco da tabela id_tabela já está no buffer. Se estiver, retorna um ponteiro para ele. Se não estiver, lê o bloco do disco com getBlock, copia para o primeiro slot livre do buffer, atualiza o header dele e retorna um ponteiro para esse slot
// daí agora todo lugar que chama getBlock() tem que passar a chamar bm_getBlock, imagino eu
tp_pagina *bm_getBlock(int id_tabela, int id_bloco, char *filename)
{

    // vendo se o bloco (id_bloco) da tabela (id_tabela) já está no buffer pool:
    for (int i = 0; i < bp.qtd_paginas_total; i++) {
        if (bp.header[i].id_tabela == id_tabela && bp.header[i].bloco_da_tabela == id_bloco) {
            printf("bm_getBlock: bloco %d da tabela com id %d e filename %s JÁ ESTÁ no buffer\n", id_bloco, id_tabela, filename);
            return &bp.paginas[i]; // página já está no buffer
        }
    }

    // se a página não estiver no buffer, trazemos ela do disco e colocamos no buffer

    // mas antes precisamos ver se tem espaço no buffer pra colocar a página:
    int indice_disponivel = -1; // slot disponível no buffer pool
    for (int i = 0; i < bp.qtd_paginas_total; i++)
    {
        if (bp.header[i].id_tabela == -1)
        { // -1 = livre
            indice_disponivel = i;
            break;
        }
    }

    // se o buffer tiver cheio, o professor disse que podíamos dar panic (que não precisávamos implementar uma política de trocas):
    if (indice_disponivel == -1)
    {
        // printf("ERROR: buffer pool cheio\n");
        printf("BUFFER POOL CHEIO\n");
        struct fs_objects objeto_temporario = leObjeto(filename); // é melhor eu transformar aqui do que em bm_writeBufferToDisk, pq depois pra passar nas outras funções por parametro é mais caótico
        bm_writeBufferToDisk(&objeto_temporario);
        // exit(1);
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
    strcpy(bp.header[indice_disponivel].filename, filename);
    bp.qtd_paginas_ocupadas++;
    bp.qtd_paginas_desocupadas--;

    return &bp.paginas[indice_disponivel];
}

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

// função intermediária do WriteBufferToDisk (ele não pode ser acessado diretamente)

// função que gerencia a saída de uma página do buffer pool, organizando sua escrita no disco
tp_pagina *bm_writeBufferToDisk(struct fs_objects *objeto)
{

    // variável para guardar o valor do índice da página pro return, pq é melhor quando entrar na função já ter um valor pra começar, sem ter que ficar procurando
    int indice_pagina = 0;

    while (pagina_da_vez_para_sair < (bp.qtd_paginas_total - 1))
    {
        printf("pc= %d e db= %d\n", bp.header[pagina_da_vez_para_sair].db, bp.header[pagina_da_vez_para_sair].pc);
        if (pagina_da_vez_para_sair > (bp.qtd_paginas_total - 1))
        { // se pagina_da_vez_para_sair é maior que a ultima pagina, retorna para 0
            pagina_da_vez_para_sair = 0;
        }
        else if (bp.header[pagina_da_vez_para_sair].db == 0 && bp.header[pagina_da_vez_para_sair].pc == 0)
        { // se dirty bit e pin count da pagina que vai sair é 0, é pq não sofreu alterações e só tira a página

            printf("Pagina de indice %d foi escolhida para sair\n", pagina_da_vez_para_sair);
            indice_pagina = pagina_da_vez_para_sair;
            pagina_da_vez_para_sair++;

            return &bp.paginas[indice_pagina];
        }
        else if (bp.header[pagina_da_vez_para_sair].db == 1 && bp.header[pagina_da_vez_para_sair].pc == 0)
        { // if dirty bit da pagina que vai sair é 1 e e pin count  é 0, tem que escrveer no disco antes de tirar a págian

            writeBufferToDisk(&bp.paginas[pagina_da_vez_para_sair], objeto);
            printf("Pagina de indice %d foi escolhida para sair\n", pagina_da_vez_para_sair);
            indice_pagina = pagina_da_vez_para_sair;
            pagina_da_vez_para_sair++;

            return &bp.paginas[indice_pagina];
        }
        pagina_da_vez_para_sair++;
    }
    return NULL;
}



// criando uma página nova diretamente no pool (antes, quando era criada uma nova página, ela era alocada em um lugar qualquer da memória e logo escrita no disco, mas agora colocamos ela no buffer e somente no buffer quando ela é criada)
// só é usada na operação de INSERT, quando é o primeiro bloco da tabela ou quando o bloco atual está cheio e precisamos criar um novo bloco (que agora é criado diretamente no buffer e sem ser escrito no disco imediatamente: por um tempo, a página fica só no buffer sem estar no disco)
tp_pagina *bm_novaPaginaNoBuffer(int id_tabela, int id_bloco, char *filename) {
    
    // procurando um slot livre no buffer pool:
    int indice_disponivel = -1;
    for (int i = 0; i < bp.qtd_paginas_total; i++) {
        if (bp.header[i].id_tabela == -1) {
            indice_disponivel = i;
            break;
        }
    }

    if (indice_disponivel == -1) {
        printf("ERROR: buffer pool cheio\n");
        exit(1);
    }

    // inicializando a página:
    bp.paginas[indice_disponivel].id = (unsigned int)id_bloco;
    bp.paginas[indice_disponivel].nrec = 0;
    bp.paginas[indice_disponivel].position = 0;

    // atualizando o header:
    bp.header[indice_disponivel].id_tabela = id_tabela;
    bp.header[indice_disponivel].bloco_da_tabela = id_bloco;
    bp.header[indice_disponivel].db = 1; // db=1 desde o início porque ela precisa ser gravada no disco quando o programa terminar ou quando ela for substituída, visto que ela não é mais gravada no disco logo depois da criação
    bp.header[indice_disponivel].pc = 1;
    strcpy(bp.header[indice_disponivel].filename, filename);
    bp.qtd_paginas_ocupadas++;
    bp.qtd_paginas_desocupadas--;

    printf("bm_novaPaginaNoBuffer: novo bloco (bloco %d) da tabela %d está sendo criado no buffer pool no slot %d\n", id_bloco, id_tabela, indice_disponivel);

    return &bp.paginas[indice_disponivel];
}

// criei uma função para colocar o dirty bit da página como 1, pois colocar no meio do código estava poluindo muito o código
void bm_marcarDirtyBit(tp_pagina *pagina) {
    for (int i = 0; i < bp.qtd_paginas_total; i++) {
        if (&bp.paginas[i] == pagina) { // compara os endereços
            bp.header[i].db = 1;
            break;
        }
    }
}