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

static int pagina_da_vez_para_sair = 0; // variável que vai guardar o índice da página pra expulsão de página em relógio no buffer pool. começo com 0 pq a primeira página a ser escrita é a 0. Só entra em ação quando buffer está cheio ou usuário faz exit

static int indice_pagina_para_subtituir; // variável que guarda para bm_novaPaginaNoBuffer o indice da nova página do buffer pool para ser usada (página reiniciada)

// intermediário para o getBlock (testei os comandos (insert, delete...) e funcionou normalmente. As impressões que dizem se o bloco está ou não no buffer parecem funcionar. Mas não criei uma função de imprimir o buffer pra conferir o buffer)
// verifica se o bloco id_bloco da tabela id_tabela já está no buffer. Se estiver, retorna um ponteiro para ele. Se não estiver, lê o bloco do disco com getBlock, copia para o primeiro slot livre do buffer, atualiza o header dele e retorna um ponteiro para esse slot
// daí agora todo lugar que chama getBlock() tem que passar a chamar bm_getBlock, imagino eu
tp_pagina *bm_getBlock(int id_tabela, int id_bloco, char *filename)
{

    // vendo se o bloco (id_bloco) da tabela (id_tabela) já está no buffer pool:
    for (int i = 0; i < bp.qtd_paginas_total; i++)
    {
        if (bp.header[i].id_tabela == id_tabela && bp.header[i].bloco_da_tabela == id_bloco)
        {
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
        struct fs_objects objeto_temporario = leObjetoById(id_tabela); // é melhor eu transformar aqui do que em bm_writeBufferToDisk, pq depois pra passar nas outras funções por parametro é mais caótico
        // bm_writeBufferToDisk(&objeto_temporario);
        if (bm_writeBufferToDisk(&objeto_temporario) == 0) // verificar
            return NULL;

        indice_disponivel = indice_pagina_para_subtituir;
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
    bp.header[indice_disponivel].pc = 0;

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
    indice_pagina_para_subtituir = algoritmo_clock();
    bm.pagina = &bp.paginas[indice_pagina_para_subtituir]; // buffer manager aponta para a página que vai sair

    if (indice_pagina_para_subtituir == -1)
    {
        printf("NAO HA PAGINAS NO BUFFER PARA SUBSTITUIR\n");
        return NULL;
    }

    if (bp.header[indice_pagina_para_subtituir].db == 1 && bp.header[indice_pagina_para_subtituir].pc == 0)
    { // só entra aqui se a página vai ser escrita realmente no disco
        writeBufferToDisk(bm.pagina, objeto);
    }

    return bm.pagina;
}

int algoritmo_clock()
{ // retorna id do bloco substituido
    // variável para guardar o valor do índice da página pro return, pq é melhor quando entrar na função já ter um valor pra começar, sem ter que ficar procurando
    int indice_pagina = 0;

    while (pagina_da_vez_para_sair < bp.qtd_paginas_total)
    {
        printf("db= %d e pc= %d\n", bp.header[pagina_da_vez_para_sair].db, bp.header[pagina_da_vez_para_sair].pc);
        if (pagina_da_vez_para_sair >= bp.qtd_paginas_total)
        { // se pagina_da_vez_para_sair é maior que a ultima pagina, retorna para 0
            pagina_da_vez_para_sair = 0;
        }
        else if (bp.header[pagina_da_vez_para_sair].db == 0 && bp.header[pagina_da_vez_para_sair].pc == 0)
        { // se dirty bit e pin count da pagina que vai sair é 0, é pq não sofreu alterações e só tira a página

            printf("Pagina de indice %d foi escolhida para sair\n", pagina_da_vez_para_sair);
            indice_pagina = pagina_da_vez_para_sair;
            pagina_da_vez_para_sair++;

            return indice_pagina;
        }
        else if (bp.header[pagina_da_vez_para_sair].db == 1 && bp.header[pagina_da_vez_para_sair].pc == 0)
        { // if dirty bit da pagina que vai sair é 1 e e pin count  é 0, tem que escrveer no disco antes de tirar a págian

            printf("Pagina de indice %d foi escolhida para sair\n", pagina_da_vez_para_sair);
            indice_pagina = pagina_da_vez_para_sair;
            pagina_da_vez_para_sair++;

            return indice_pagina;
        }
        pagina_da_vez_para_sair++;
    }
    return -1;
}

// criando uma página nova diretamente no pool (antes, quando era criada uma nova página, ela era alocada em um lugar qualquer da memória e logo escrita no disco, mas agora colocamos ela no buffer e somente no buffer quando ela é criada)
// só é usada na operação de INSERT, quando é o primeiro bloco da tabela ou quando o bloco atual está cheio e precisamos criar um novo bloco (que agora é criado diretamente no buffer e sem ser escrito no disco imediatamente: por um tempo, a página fica só no buffer sem estar no disco)
tp_pagina *bm_novaPaginaNoBuffer(int id_tabela, int id_bloco, char *filename)
{

    // procurando um slot livre no buffer pool:
    int indice_disponivel = -1;
    for (int i = 0; i < bp.qtd_paginas_total; i++)
    {
        if (bp.header[i].id_tabela == -1)
        {
            indice_disponivel = i;
            break;
        }
    }

    if (indice_disponivel == -1)
    {
        printf("ERROR: buffer pool cheio\n");
        struct fs_objects objeto_temporario = leObjetoById(id_tabela); // é melhor eu transformar aqui do que em bm_writeBufferToDisk, pq depois pra passar nas outras funções por parametro é mais caótico
        if (!bm_writeBufferToDisk(&objeto_temporario))
        {
            return NULL;
        }

        // bm_gravarTodasAsPaginasDoBufferNoDisco(); // como aqui tem exit também, precisamos gravar todas as páginas sujas no disco antes de dar exit, senão perdemos os dados que estão só no buffer
        indice_disponivel = indice_pagina_para_subtituir;
        //   exit(1);
    }

    // id_bloco = bp.paginas[indice_disponivel].id++;
    //   inicializando a página:
    bp.header[indice_disponivel].pc = 1;
    bp.paginas[indice_disponivel].id = (unsigned int)id_bloco;
    bp.paginas[indice_disponivel].nrec = 0;
    bp.paginas[indice_disponivel].position = 0;

    // atualizando o header:
    bp.header[indice_disponivel].id_tabela = id_tabela;
    bp.header[indice_disponivel].bloco_da_tabela = id_bloco;
    bp.header[indice_disponivel].db = 1; // db=1 desde o início porque ela precisa ser gravada no disco quando o programa terminar ou quando ela for substituída, visto que ela não é mais gravada no disco logo depois da criação
    bp.header[indice_disponivel].pc = 0;

    strcpy(bp.header[indice_disponivel].filename, filename);
    bp.qtd_paginas_ocupadas++;
    bp.qtd_paginas_desocupadas--;

    printf("bm_novaPaginaNoBuffer: novo bloco (bloco %d) da tabela %d está sendo criado no buffer pool no slot %d\n", id_bloco, id_tabela, indice_disponivel);

    return &bp.paginas[indice_disponivel];
}

// criei uma função para colocar o dirty bit da página como 1, pois colocar no meio do código estava poluindo muito o código
void bm_marcarDirtyBit(tp_pagina *pagina)
{
    for (int i = 0; i < bp.qtd_paginas_total; i++)
    {
        if (&bp.paginas[i] == pagina)
        { // compara os endereços
            bp.header[i].db = 1;
            break;
        }
    }
}

// grava no disco todas as páginas com db=1 no exit, quando o programa terminar
void bm_gravarTodasAsPaginasDoBufferNoDisco()
{

    printf("\nbm_gravarTodasAsPaginasDoBufferNoDisco: como o programa terminou, gravando páginas sujas no disco...\n");

    for (int i = 0; i < bp.qtd_paginas_total; i++)
    {

        if (bp.header[i].id_tabela == -1 || bp.header[i].db == 0)
        {
            continue; // se o slot estiver livre ou a página não tiver sido modificada, pula
        }

        // me baseei no finalizaInsert pra gravar no disco as páginas do buffer:
        // ele FAZ primeiro fopen(directory, "r+b") // isso ele continua fazendo (temos que remover isso de finalizaInsert? Acho que sim, né, porque ele não devia fazer fopen sem passar pelo buffer manager, só temos que ver como fazer isso e as implicações)
        // depois FAZIA fseek(dados, buffer->id * sizeof(tp_pagina), SEEK_SET) // isso já comentei (ele não faz mais) -> então tô fazendo isso aqui, porque é aqui que vamos gravar no disco as páginas
        // depois ele FAZIA fwrite(buffer, sizeof(tp_pagina), 1, dados) // isso ele também não faz mais (eu comentei onde ele fazia isso) -> então tô fazendo isso aqui também pelo mesmo motivo
        FILE *arquivo_inteiro_tabela = fopen(bp.header[i].filename, "r+b");
        if (!arquivo_inteiro_tabela)
        {
            printf("ERRO: bm_gravarTodasAsPaginasDoBufferNoDisco: não foi possível abrir o arquivo %s\n", bp.header[i].filename);
            continue;
        }
        fseek(arquivo_inteiro_tabela, (long)bp.paginas[i].id * sizeof(tp_pagina), SEEK_SET);
        fwrite(&bp.paginas[i], sizeof(tp_pagina), 1, arquivo_inteiro_tabela);
        fclose(arquivo_inteiro_tabela);

        printf("bm_gravarTodasAsPaginasDoBufferNoDisco: bloco %d da tabela com id/código %d gravado no arquivo %s\n", bp.header[i].bloco_da_tabela, bp.header[i].id_tabela, bp.header[i].filename);

        // será que tem quer zerar por completo o buffer também? Aqui que não, né? Porque é uma variável e ela é automaticamente excluida quando o programa termina
        bp.header[i].db = 0; // fazendo isso só por lógica, mas nem precisa eu acho // colocando o dirty bit como 0, porque agora a página foi colocada no disco, ou seja, a página que está no buffer agora está igual ao bloco que está no disco
    }
    printf("bm_gravarTodasAsPaginasDoBufferNoDisco: todas as páginas com dirty bit igual a 1 foram gravadas no disco\n");
}
