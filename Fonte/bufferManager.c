#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "types.h"
#include "dictionary.h"
#include "buffer.h"
#include "bufferManager.h"

static int pagina_da_vez_para_sair = 0; // variável que vai guardar o índice da página pra expulsão de página em relógio no buffer pool. 
static int indice_pagina_para_subtituir; // variável que guarda para bm_novaPaginaNoBuffer o indice da nova página do buffer pool para ser usada (página reiniciada)

// função hash não ordenável
// copiei da wikipedia see: https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
static uint64_t FNV_1ahash(buffer_key key) {
    uint64_t hash = 14695981039346656037ULL;
    uint64_t FNV_prime = 1099511628211ULL;

    uint8_t *bytes = (uint8_t *)&key;
    for (int i = 0; i < 8; i++){
        hash ^= bytes[i];
        hash *= FNV_prime;
    }
    
    return hash;
}

static void setSlot(buffer_key *key, int indiceBuffer){
    uint64_t hash = FNV_1ahash(*key);
    int capacity = (2 * PAGES);
    int i = hash % capacity;

    while (bp.hash_directory[i].buffer_id != HASHNULL && bp.hash_directory[i].buffer_id != HASHDELETED){
        i = (i + 1) % capacity;
    }

    bp.hash_directory[i].buffer_id = indiceBuffer;
    bp.hash_directory[i].key = *key;
    return;
}

static inline int key_equals(buffer_key *a, buffer_key *b) {
    return a->block == b->block && a->table == b->table && a->data_base == b->data_base;
}

static int getSlot(buffer_key *key){
    uint64_t hash = FNV_1ahash(*key);
    int capacity = (2 * PAGES);
    int i = hash % capacity;

    int j = 0;
    while (j < capacity){
        int id = bp.hash_directory[i].buffer_id;

        if (id == HASHNULL) {
            return -1;
        }

        if (id >= 0 && key_equals(&bp.hash_directory[i].key, key)) {
            return id; 
        }

        i = (i + 1) % capacity;
        j++;
    }
    
    return -1;
}

// intermediário para o getBlock (testei os comandos (insert, delete...)
tp_pagina *bm_getBlock(int id_tabela, int id_bloco, char *filename)
{
    buffer_key key = {(uint32_t) id_bloco, (uint16_t) id_tabela, 0};
    int indice_disponivel = getSlot(&key);
  
    if (indice_disponivel != -1 ){
        DEBUG_PRINT("bm_getBlock: bloco %d da tabela com id %d e filename %s JÁ ESTÁ no buffer\n", id_bloco, id_tabela, filename);
        return &bp.paginas[indice_disponivel]; // página já está no buffer
    }

    // se a página não estiver no buffer, trazemos ela do disco e colocamos no buffer
    for (int i = 0; i < bp.qtd_paginas_total; i++)
    {
        if (bp.header[i].id_tabela == -1)
        { // -1 = livre
            indice_disponivel = i;
            break;
        }
    }

    if (indice_disponivel == -1)
    {
        // printf("ERROR: buffer pool cheio\n");
        DEBUG_PRINT("BUFFER POOL CHEIO\n");

        if (bm_writeBufferToDisk() == NULL)
            return NULL;

        indice_disponivel = indice_pagina_para_subtituir;
        // exit(1);
    }

    DEBUG_PRINT("bm_getBlock: bloco %d da tabela com id %d e filename %s NÃO ESTÁ no buffer e será lido do disco\n", id_bloco, id_tabela, filename);
    tp_pagina *bloco = getBlock((unsigned int)id_bloco, filename);
    // nao temos diretorios mapeados com index por enquanto
    setSlot(&key, indice_disponivel);
    bp.paginas[indice_disponivel] = *bloco;

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
    DEBUG_PRINT("\n------ Header Buffer Pool ------\n");
    DEBUG_PRINT("total: %d | ocupados: %d | livres: %d\n\n", bp.qtd_paginas_total, bp.qtd_paginas_ocupadas, bp.qtd_paginas_desocupadas);
    DEBUG_PRINT("slot       id_tabela  bloco    dp      pc\n");
    for (int i = 0; i < bp.qtd_paginas_total; i++)
    {
        if (bp.header[i].id_tabela == -1)
        {
            continue; // pulando slots livres
        }
        DEBUG_PRINT("%d          %d          %d        %d       %d\n", i, bp.header[i].id_tabela, bp.header[i].bloco_da_tabela, bp.header[i].db, bp.header[i].pc);
    }
    DEBUG_PRINT("---------------------------------\n");
}

// função intermediária do WriteBufferToDisk (ele não pode ser acessado diretamente)
// função que gerencia a saída de uma página do buffer pool, organizando sua escrita no disco
tp_pagina *bm_writeBufferToDisk()
{
    indice_pagina_para_subtituir = algoritmo_clock();
    bm.pagina = &bp.paginas[indice_pagina_para_subtituir]; // buffer manager aponta para a página que vai sair
    int id_da_Tabela = bp.header[indice_pagina_para_subtituir].id_tabela;
    struct fs_objects objeto = leObjetoById(id_da_Tabela);

    if (indice_pagina_para_subtituir == -1)
    {
        DEBUG_PRINT("NAO HA PAGINAS NO BUFFER PARA SUBSTITUIR\n");
        return NULL;
    }

    if (bp.header[indice_pagina_para_subtituir].db == 1 && bp.header[indice_pagina_para_subtituir].pc == 0)
    { // só entra aqui se a página vai ser escrita realmente no disco
        writeBufferToDisk(bm.pagina, &objeto);
    }

    //   inicializando a página:
    bp.header[indice_pagina_para_subtituir].pc = 1;
    // bp.paginas[indice_pagina_para_subtituir].id = (unsigned int)id_bloco;
    bp.paginas[indice_pagina_para_subtituir].nrec = 0;
    bp.paginas[indice_pagina_para_subtituir].position = 0;
    bp.hash_directory[indice_pagina_para_subtituir].buffer_id = HASHDELETED;
    // atualizando o header:
    bp.header[indice_pagina_para_subtituir].id_tabela = -1;
    bp.header[indice_pagina_para_subtituir].bloco_da_tabela = -1;
    bp.header[indice_pagina_para_subtituir].db = 0;
    bp.header[indice_pagina_para_subtituir].pc = 0;

    bp.qtd_paginas_ocupadas--;
    bp.qtd_paginas_desocupadas++;

    return bm.pagina;
}

int algoritmo_clock()
{ // retorna id do bloco substituido
  // variável para guardar o valor do índice da página pro return, pq é melhor quando entrar na função já ter um valor pra começar, sem ter que ficar procurando
    int indice_pagina = 0;

    while (1)
    {
        if (pagina_da_vez_para_sair >= bp.qtd_paginas_total){ 
            pagina_da_vez_para_sair = 0;
        }
        else if (bp.header[pagina_da_vez_para_sair].db == 0 && bp.header[pagina_da_vez_para_sair].pc == 0)
        { // se dirty bit e pin count da pagina que vai sair é 0, é pq não sofreu alterações e só tira a página

            DEBUG_PRINT("Pagina de indice %d foi escolhida para sair\n", pagina_da_vez_para_sair);
            indice_pagina = pagina_da_vez_para_sair;
            pagina_da_vez_para_sair++;

            return indice_pagina;
        }
        else if (bp.header[pagina_da_vez_para_sair].db == 1 && bp.header[pagina_da_vez_para_sair].pc == 0)
        { // if dirty bit da pagina que vai sair é 1 e e pin count  é 0, tem que escrveer no disco antes de tirar a págian

            DEBUG_PRINT("Pagina de indice %d foi escolhida para sair\n", pagina_da_vez_para_sair);
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
        DEBUG_PRINT("ERROR: buffer pool cheio\n");
        if (bm_writeBufferToDisk() == NULL)
        {
            return NULL;
        }

        indice_disponivel = indice_pagina_para_subtituir;    }

    // inicializando a página:
    bp.paginas[indice_disponivel].id = (unsigned int)id_bloco;
    bp.paginas[indice_disponivel].nrec = 0;
    bp.paginas[indice_disponivel].position = 0;
    buffer_key key = {(uint32_t) id_bloco, (uint16_t) id_tabela, 0};
    setSlot(&key, indice_disponivel);
    // atualizando o header:
    bp.header[indice_disponivel].id_tabela = id_tabela;
    bp.header[indice_disponivel].bloco_da_tabela = id_bloco;
    bp.header[indice_disponivel].db = 1; // db=1 desde o início porque ela precisa ser gravada no disco quando o programa terminar ou quando ela for substituída, visto que ela não é mais gravada no disco logo depois da criação
    bp.header[indice_disponivel].pc = 1;
    strcpy(bp.header[indice_disponivel].filename, filename);
    bp.qtd_paginas_ocupadas++;
    bp.qtd_paginas_desocupadas--;

    DEBUG_PRINT("bm_novaPaginaNoBuffer: novo bloco (bloco %d) da tabela %d está sendo criado no buffer pool no slot %d\n", id_bloco, id_tabela, indice_disponivel);

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

// despina a página (pc=0) quando quem pegou ela com bm_getBlock ou bm_novaPaginaNoBuffer termina de usar:
void bm_despinarPagina(tp_pagina *pagina) {
    for (int i = 0; i < bp.qtd_paginas_total; i++) {
        if (&bp.paginas[i] == pagina) { // compara os endereços
            bp.header[i].pc = 0;
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

        if (bp.header[i].id_tabela == -1 || bp.header[i].db == 0){
            continue; // se o slot estiver livre ou a página não tiver sido modificada, pula
        }

        FILE *arquivo_inteiro_tabela = fopen(bp.header[i].filename, "r+b");
        if (!arquivo_inteiro_tabela) {
            DEBUG_PRINT("ERRO: bm_gravarTodasAsPaginasDoBufferNoDisco: não foi possível abrir o arquivo %s\n", bp.header[i].filename);
            continue;
        }
        fseek(arquivo_inteiro_tabela, (long)bp.paginas[i].id * sizeof(tp_pagina), SEEK_SET);
        fwrite(&bp.paginas[i], sizeof(tp_pagina), 1, arquivo_inteiro_tabela);
        fclose(arquivo_inteiro_tabela);

        DEBUG_PRINT("bm_gravarTodasAsPaginasDoBufferNoDisco: bloco %d da tabela com id/código %d gravado no arquivo %s\n", bp.header[i].bloco_da_tabela, bp.header[i].id_tabela, bp.header[i].filename);

        // será que tem quer zerar por completo o buffer também? Aqui que não, né? Porque é uma variável e ela é automaticamente excluida quando o programa termina
        bp.header[i].db = 0; // fazendo isso só por lógica, mas nem precisa eu acho // colocando o dirty bit como 0, porque agora a página foi colocada no disco, ou seja, a página que está no buffer agora está igual ao bloco que está no disco
    }
    DEBUG_PRINT("bm_gravarTodasAsPaginasDoBufferNoDisco: todas as páginas com dirty bit igual a 1 foram gravadas no disco\n");
}
