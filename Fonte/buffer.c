#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memoryContext.h"

#ifndef FMACROS // garante que macros.h não seja reincluída
#include "macros.h"
#endif
///
#ifndef FTYPES // garante que types.h não seja reincluída
#include "types.h"
#endif

#include "misc.h"
#include "dictionary.h"

static int isDeleted(char *linha);

//// imprime os dados no buffer (deprecated?)

int print_tabela_bloco(tp_pagina *pagina, tp_table *s, struct fs_objects objeto, int num_page)
{ // printa as tuplas de uma tabela em uma determinada página (testei após fazer um insert)

    int aux, i, num_reg = objeto.qtdCampos;

    if (pagina->nrec == 0)
    {
        return ERRO_IMPRESSAO;
    }

    i = aux = 0;
    printf("-------- Impressao da tabela %d de nome %s \n", objeto.cod, objeto.nome);

    aux = cabecalho(s, num_reg);

    while (i < pagina->nrec)
    { // Enquanto i < numero de registros * tamanho de uma instancia da tabela

        drawline(pagina, s, objeto, i, num_page);
        i++;
    }
    return SUCCESS;
}

// tp_pagina* initBuffer(unsigned int id){
//     tp_pagina *buffer = uffslloc(sizeof(tp_pagina));

//     if (buffer == NULL) {
//         printf("ERROR: Memory allocation failed.\n\n");
//         return NULL;
//     }

//     buffer->id = id;
//     return buffer;
// }

// tem que inicializar a página também (era o antigo initBuffer)
tp_pagina *initPagina(unsigned int id)
{
    tp_pagina *pagina = uffslloc(sizeof(tp_pagina));
    if (pagina == NULL)
    {
        printf("ERROR: Memory allocation failed.\n\n");
        return NULL;
    }
    pagina->id = id;
    // inicializando esses também:
    pagina->nrec = 0;
    pagina->position = 0;

    return pagina;
}

// passando qtd_paginas pois o professor disse para passar a quantidade de páginar que o buffer pool é pra ter
void initBufferPool(int qtd_paginas)
{
    if (qtd_paginas > PAGES)
    {
        printf("ERRO: A quantidade máxima de páginas é %d.\n", PAGES);
        return;
    }
    bp.qtd_paginas_total = qtd_paginas;
    bp.qtd_paginas_ocupadas = 0;
    bp.qtd_paginas_desocupadas = qtd_paginas;

    // inicializando os slots do header do buffer (indicando que todos os frames da área de dados estão livres):
    for (int i = 0; i < qtd_paginas; i++)
    {
        bp.header[i].id_tabela = -1; // -1 = slot livre
        bp.header[i].bloco_da_tabela = -1;
        bp.header[i].db = 0;
        bp.header[i].pc = 0;
    }
}

// serve pra todas as operação que precisam trazer um bloco do disco (SELECT, DELETE e UPDATE)
// toda vez que ele é chamado, ele lê do disco
tp_pagina *getBlock(unsigned int id, char *filename)
{
    // TODO: change how the file is handled; repeatedly opening and closing it is inefficient (não é top)
    FILE *fd = fopen(filename, "r+"); // abre o arquivo da tabela
    // pergunta: não tem que fechar o arquivo depois? Ou ele já é fechado em algum outro lugar?

    if (!fd)
    {
        printf("ERROR: failed to open %s", filename);
        return NULL;
    }

    long int pos = (long int)id * sizeof(tp_pagina); // se quiser o primeiro bloco do arquivo, id = 0, se quiser o segundo bloco do arquivo, id = 1, e assim por diante
    fseek(fd, pos, SEEK_SET);                        // pula pra posição do bloco id
    tp_pagina *pagina = uffslloc(sizeof(tp_pagina));
    fread(pagina, sizeof(tp_pagina), 1, fd); // lê os bytes do disco
    return pagina;                           // retorna os bytes do bloco
}

// RETORNA PAGINA DO BUFFER
PageResult *getPage(tp_table *campos, struct fs_objects objeto, int page)
{

    if (page >= PAGES || page < 0)
        return ERRO_PAGINA_INVALIDA;

    char directory[LEN_DB_NAME_IO];
    strcpy(directory, connected.db_directory);
    strcat(directory, objeto.nArquivo);

    tp_pagina *pagina = getBlock((unsigned int)page, directory);

    tupla *tuplas = (tupla *)uffslloc(sizeof(tupla) * (pagina->nrec)); // Aloca a quantidade de tuplas necessária

    if (!tuplas)
        return ERRO_DE_ALOCACAO;

    int indiceTupla = 0, i = 0;

    if (!pagina->position)
        return NULL;

    char *nullos = (char *)uffslloc(objeto.qtdCampos * sizeof(char));

    while (i < pagina->position)
    {

        if (isDeleted(pagina->data + i))
        {
            i += tamTupla(campos, objeto);
            continue;
        }
        tuplas[indiceTupla].offset = i;
        tuplas[indiceTupla].ncols = objeto.qtdCampos;
        i++; // para o byte de deleted
        memcpy(nullos, pagina->data + i, objeto.qtdCampos);
        i += objeto.qtdCampos;

        tuplas[indiceTupla].column = (column *)uffslloc(sizeof(column) * objeto.qtdCampos);
        tuplas[indiceTupla].bufferPage = page;
        for (int ic = 0; ic < objeto.qtdCampos; ic++)
        {
            column *c = &tuplas[indiceTupla].column[ic];

            c->tipoCampo = campos[ic].tipo;
            strcpy(c->nomeCampo, campos[ic].nome); // Guarda nome do campo
            if (nullos[ic])
                c->valorCampo = COLUNA_NULL;
            else
            {
                c->valorCampo = (char *)uffslloc(sizeof(char) * campos[ic].tam + 1);
                memcpy(c->valorCampo, pagina->data + i, campos[ic].tam);
                c->valorCampo[campos[ic].tam] = '\0';
            }
            i += campos[ic].tam;
        }

        indiceTupla++;
    }
    PageResult *pg = (PageResult *)uffslloc(sizeof(PageResult));
    pg->tuplas = tuplas;
    pg->nrec = indiceTupla;

    return pg; // Retorna a 'page' do buffer
}

// EXCLUIR TUPLA BUFFER
column *excluirTuplaBuffer(tp_pagina *pagina, tp_table *campos, struct fs_objects objeto, int page, int nTupla)
{
    column *tuplas = (column *)uffslloc(sizeof(column) * objeto.qtdCampos);

    if (tuplas == NULL)
        return ERRO_DE_ALOCACAO;

    if (pagina[page].nrec == 0) // Essa página não possui registros
        return ERRO_PARAMETRO;

    int i, tamTpl = tamTupla(campos, objeto), j = 0, t = 0;
    i = tamTpl * nTupla; // Calcula onde começa o registro

    while (i < tamTpl * nTupla + tamTpl)
    {
        t = 0;

        tuplas[j].valorCampo = (char *)uffslloc(sizeof(char) * campos[j].tam); // Aloca a quantidade necessária para cada campo
        tuplas[j].tipoCampo = campos[j].tipo;                                  // Guarda o tipo do campo
        strcpylower(tuplas[j].nomeCampo, campos[j].nome);                      // Guarda o nome do campo

        while (t < campos[j].tam)
        {
            tuplas[j].valorCampo[t] = pagina[page].data[i]; // Copia os dados
            t++;
            i++;
        }
        j++;
    }
    j = i;
    i = tamTpl * nTupla;
    for (; i < pagina[page].position; i++, j++) // Desloca os bytes do pagina sobre a tupla excluida
        pagina[page].data[i] = pagina[page].data[j];

    pagina[page].position -= tamTpl;
    pagina[page].nrec--;

    return tuplas; // Retorna a tupla excluida do pagina
}
// INSERE UMA TUPLA NO pagina!
char *getTupla(tp_table *campos, struct fs_objects objeto, int from)
{ // Pega uma tupla do disco a partir do valor de from
    // + qtdCampos para os bytes de coluna null e +1 para o byte de tupla valida
    int tamTpl = tamTupla(campos, objeto);
    char *linha = (char *)uffslloc(sizeof(char) * tamTpl);

    FILE *dados;
    from = from * tamTpl;
    char directory[LEN_DB_NAME_IO];
    strcpy(directory, connected.db_directory);
    strcat(directory, objeto.nArquivo);

    dados = fopen(directory, "r");

    if (dados == NULL)
    {
        return ERRO_DE_LEITURA;
    }

    fseek(dados, from, SEEK_CUR);
    if (fgetc(dados) == EOF)
    {
        fclose(dados);
        return ERRO_DE_LEITURA;
    }

    fseek(dados, -1, SEEK_CUR);
    fread(linha, sizeof(char), tamTpl, dados); // Traz a tupla inteira do arquivo

    fclose(dados);
    return linha;
}
/////
void setTupla(tp_pagina *pagina, char *tupla, int tam, int pos)
{ // Coloca uma tupla de tamanho "tam" no buffer e na página "pos"
    int i = pagina[pos].position;
    for (; i < pagina[pos].position + tam; i++)
        pagina[pos].data[i] = *(tupla++);
}
//// insere uma tupla no buffer
int colocaTuplaBuffer(tp_pagina *pagina, int from, tp_table *campos, struct fs_objects objeto)
{ // Define a página que será incluida uma nova tupla
    int i, found;
    char *tupla = getTupla(campos, objeto, from);
    if (tupla == ERRO_DE_LEITURA)
        return ERRO_LEITURA_DADOS;

    int tam = tamTupla(campos, objeto);

    for (i = found = 0; !found && i < PAGES; i++)
    { // Procura pagina com espaço para a tupla.
        if (SIZE - pagina[i].position > tam)
        { // Se na pagina i do buffer tiver espaço para a tupla, coloca tupla.
            setTupla(pagina, tupla, tam, i);
            found = 1;
            pagina[i].position += tam; // Atualiza proxima posição vaga dentro da pagina.
            if (isDeleted(tupla))
            {
                return ERRO_LEITURA_DADOS_DELETADOS;
            }
            pagina[i].nrec++;
        }
    }
    return found ? SUCCESS : ERRO_BUFFER_CHEIO;
}
////////

void cria_campo(int tam, int header, char *val, int x)
{
    int i;
    char aux[30];
    if (header)
    {
        for (i = 0; i <= 30 && val[i] != '\0'; i++)
            aux[i] = val[i];
        for (; i < 30; i++)
            aux[i] = ' ';
        aux[i] = '\0';
        printf("%s", aux);
        return;
    }
    for (i = 0; i < x; i++)
        printf(" ");
}

/* ----------------------------------------------------------------------------------------------
    Objetivo:   Utilizada para gravar as mudanças do buffer no disco.
    Parametros: Buffer (tp_pagina), dados da tabela (fs_objects), número de blocos e offset do bloco.
    Retorno:    1 para sucesso, 0 para falha.
   ---------------------------------------------------------------------------------------------*/
int writeBufferToDisk(tp_pagina *pagina, struct fs_objects *objeto)
{
    int success = 1; // flag de sucesso porque sucesso deveria valer 1 não 0!
    char directory[LEN_DB_NAME_IO];
    strcpy(directory, connected.db_directory);
    strcat(directory, objeto->nArquivo);

    if (pagina == NULL)
    {
        printf("ERROR: empty buffer\n");
        return 0;
    }

    FILE *dados = fopen(directory, "r+b");
    if (!dados)
    {
        printf("ERROR: Unable to open file for writing.\n");
        return 0;
    }

    // seek(dados, pagina->id *sizeof(tp_pagina), SEEK_SET);
    fseek(dados, pagina->id * sizeof(tp_pagina), SEEK_SET);
    // // TEMOS QUE AJUSTAR:
    // pagina->db = 0;
    // pagina->pc = 0;
    // fazendo o que está acima:
    // descobrindo o índice da página no buffer pra atualizar o pc e o db do slot no header:
    for (int i = 0; i < bp.qtd_paginas_total; i++)
    {
        if (&bp.paginas[i] == pagina)
        { // compara os endereços
            bp.header[i].db = 0;
            bp.header[i].pc = 0;
            break;
        }
    }

    fwrite(pagina, sizeof(tp_pagina), 1, dados);
    fclose(dados);

    return success;
}

static int isDeleted(char *linha)
{
    return linha[0]; // byte se foi deletado
}

void addColumn(column **colList, column *c)
{
    c->next = NULL;
    if (*colList == NULL)
    {
        *colList = c;
        return;
    }
    column *t = *colList;
    while (t->next != NULL)
        t = t->next;

    t->next = c;
}
