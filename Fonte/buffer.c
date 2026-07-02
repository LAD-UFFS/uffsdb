#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memoryContext.h"

static BufferManager *bufferManager = NULL;

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
int printbufferpoll(tp_buffer *buffpoll, tp_table *s,struct fs_objects objeto, int num_page){

    int aux, i, num_reg = objeto.qtdCampos;

    if(buffpoll[num_page].nrec == 0){
        return ERRO_IMPRESSAO;
    }

    i = aux = 0;
    aux = cabecalho(s, num_reg);
    while(i < buffpoll[num_page].nrec){ // Enquanto i < numero de registros * tamanho de uma instancia da tabela
        drawline(buffpoll, s, objeto, i, num_page);
        i++;
    }
    return SUCCESS;
}

tp_buffer* initBuffer(unsigned int id){
    tp_buffer *buffer = uffslloc(sizeof(tp_buffer));

    if (buffer == NULL) {
        printf("ERROR: Memory allocation failed.\n\n");
        return NULL;
    }

    buffer->id = id;
    return buffer;
}

/* Inicializa o Buffer Manager e aloca o Buffer Pool */
BufferManager *initBufferManager(void){

    bufferManager = malloc(sizeof(BufferManager));

    if(bufferManager == NULL){
        printf("ERROR: Memory allocation failed.\n");
        return NULL;
    }

    bufferManager->pageSize = SIZE;
    bufferManager->nextReplacement = 0;

    bufferManager->pool = malloc(sizeof(BufferPool));

    if(bufferManager->pool == NULL){
        printf("ERROR: Memory allocation failed.\n");
        return NULL;
    }

    bufferManager->pool->maxPages = PAGES;
    bufferManager->pool->loadedPages = 0;

    bufferManager->pool->pages = malloc(sizeof(tp_buffer) * PAGES);

    if(bufferManager->pool->pages == NULL){
        printf("ERROR: Memory allocation failed.\n");
        return NULL;
    }

    for(int i = 0; i < PAGES; i++) {
        bufferManager->pool->pages[i].id = i;
        bufferManager->pool->pages[i].isOccupied = 0;
        bufferManager->pool->pages[i].nrec = 0;
        bufferManager->pool->pages[i].position = 0;
        bufferManager->pool->pages[i].db = 0;
        bufferManager->pool->pages[i].pc = 0;

        memset(bufferManager->pool->pages[i].data, 0, SIZE);
    }

    return bufferManager;

}

/* Procura uma página no Buffer Pool */
tp_buffer *findPageInBuffer(unsigned int id, const char *filename){

    if(bufferManager == NULL)
        return NULL;

    for(int i = 0; i < PAGES; i++){

        if(bufferManager->pool->pages[i].isOccupied &&
           bufferManager->pool->pages[i].id == id &&
           strcmp(bufferManager->pool->pages[i].fileName, filename) == 0){

            return &bufferManager->pool->pages[i];
        }
    }

    return NULL;
}

/*  Recupera uma página do Buffer Pool ou do arquivo de dados, caso ela ainda não esteja carregada. */
tp_buffer *getBlock(unsigned int id, char *filename){

    // Verifica se a página já está carregada no Buffer Pool
    tp_buffer *pagina = findPageInBuffer(id, filename);

    if(pagina != NULL){

        pagina->pc++;
        return pagina;
    }

    // Página não encontrada no Buffer Pool. Carrega do arquivo
    FILE *fd = fopen(filename, "r+");

    if(fd == NULL){
        printf("ERROR: failed to open %s\n", filename);
        return NULL;
    }

    long int pos = (long int)id * sizeof(tp_buffer);
    fseek(fd, pos, SEEK_SET);

    tp_buffer temp;

    if(fread(&temp, sizeof(tp_buffer), 1, fd) != 1){
        fclose(fd);
        return NULL;
    }

    fclose(fd);

    /* Procura uma posição livre no Buffer Pool. */
    for(int i = 0; i < PAGES; i++){

        if(bufferManager->pool->pages[i].isOccupied == 0){

            bufferManager->pool->pages[i] = temp;

            bufferManager->pool->pages[i].isOccupied = 1;
            bufferManager->pool->pages[i].id = id;
            bufferManager->pool->pages[i].pc = 1;

            strcpy(bufferManager->pool->pages[i].fileName, filename);

            bufferManager->pool->loadedPages++;

            return &bufferManager->pool->pages[i];
        }
    }
   
    /* Substituição de páginas (FIFO). */
    int slot = bufferManager->nextReplacement;

    bufferManager->pool->pages[slot] = temp;

    bufferManager->pool->pages[slot].isOccupied = 1;
    bufferManager->pool->pages[slot].id = id;
    bufferManager->pool->pages[slot].pc = 1;

    strcpy(bufferManager->pool->pages[slot].fileName, filename);


    // Atualiza a próxima página que poderá ser substituída
    bufferManager->nextReplacement = (bufferManager->nextReplacement + 1) % PAGES;

    return &bufferManager->pool->pages[slot];
}

// RETORNA PAGINA DO BUFFER
PageResult *getPage(tp_table *campos, struct fs_objects objeto, int page){

    if(page >= PAGES || page < 0) return ERRO_PAGINA_INVALIDA;

    
    char directory[LEN_DB_NAME_IO];
    strcpy(directory, connected.db_directory);
    strcat(directory, objeto.nArquivo);

    tp_buffer *buffer = getBlock((unsigned int) page, directory);

    if(buffer == NULL)
        return NULL;

    tupla *tuplas = (tupla *)uffslloc(sizeof(tupla) * (buffer->nrec)); //Aloca a quantidade de tuplas necessária

    if(!tuplas)
        return ERRO_DE_ALOCACAO;

    int indiceTupla = 0;
    int i = 0;

    if (!buffer->position)
        return NULL;

    char *nullos = (char *)uffslloc(objeto.qtdCampos * sizeof(char));

    while(i < buffer->position){
        
        if(isDeleted(buffer->data + i)) {
            i+=tamTupla(campos, objeto);
            continue;
        }
        tuplas[indiceTupla].offset = i; 
        tuplas[indiceTupla].ncols = objeto.qtdCampos;
        i++; //para o byte de deleted
        memcpy(nullos, buffer->data + i, objeto.qtdCampos);
        i += objeto.qtdCampos;


        tuplas[indiceTupla].column = (column *)uffslloc(sizeof(column) * objeto.qtdCampos);
        tuplas[indiceTupla].bufferPage = page;
        for (int ic = 0; ic < objeto.qtdCampos; ic++){
            column *c = &tuplas[indiceTupla].column[ic];

            c->tipoCampo = campos[ic].tipo;
            strcpy(c->nomeCampo, campos[ic].nome); //Guarda nome do campo
            if(nullos[ic]){
                c->valorCampo = COLUNA_NULL;
            } else {
                c->valorCampo = (char *)uffslloc(campos[ic].tam + 1);
                memcpy(c->valorCampo, buffer->data + i, campos[ic].tam);
                c->valorCampo[campos[ic].tam] = '\0';
            }
            i += campos[ic].tam;
        }
    
        indiceTupla++;
    }
    PageResult *pg = (PageResult *)uffslloc(sizeof(PageResult));
    pg->tuplas = tuplas;
    pg->nrec = indiceTupla;

    return pg; //Retorna a 'page' do buffer
}

// EXCLUIR TUPLA BUFFER
column * excluirTuplaBuffer(tp_buffer *buffer, tp_table *campos, struct fs_objects objeto, int page, int nTupla){
    column *tuplas = (column *)uffslloc(sizeof(column)*objeto.qtdCampos);

    if(tuplas == NULL)
        return ERRO_DE_ALOCACAO;

    if(buffer[page].nrec == 0) //Essa página não possui registros
        return ERRO_PARAMETRO;

    int i, tamTpl = tamTupla(campos, objeto), j=0, t=0;
    i = tamTpl*nTupla; //Calcula onde começa o registro

    while(i < tamTpl*nTupla+tamTpl){
        t=0;

        tuplas[j].valorCampo = (char *)uffslloc(sizeof(char)*campos[j].tam); //Aloca a quantidade necessária para cada campo
        tuplas[j].tipoCampo = campos[j].tipo;  // Guarda o tipo do campo
        strcpylower(tuplas[j].nomeCampo, campos[j].nome);   //Guarda o nome do campo

        while(t < campos[j].tam){
            tuplas[j].valorCampo[t] = buffer[page].data[i];    //Copia os dados
            t++;
            i++;
        }
        j++;
    }
    j = i;
    i = tamTpl*nTupla;
    for(; i < buffer[page].position; i++, j++) //Desloca os bytes do buffer sobre a tupla excluida
        buffer[page].data[i] = buffer[page].data[j];

    buffer[page].position -= tamTpl;
    buffer[page].nrec--;

    return tuplas; //Retorna a tupla excluida do buffer
}
// INSERE UMA TUPLA NO BUFFER!
char *getTupla(tp_table *campos,struct fs_objects objeto, int from){ //Pega uma tupla do disco a partir do valor de from
    // + qtdCampos para os bytes de coluna null e +1 para o byte de tupla valida
    int tamTpl = tamTupla(campos, objeto); 
    char *linha=(char *)uffslloc(sizeof(char)*tamTpl);

    FILE *dados;
    from = from * tamTpl;
	char directory[LEN_DB_NAME_IO];
    strcpy(directory, connected.db_directory);
    strcat(directory, objeto.nArquivo);

    dados = fopen(directory, "r");

    if (dados == NULL) {
        return ERRO_DE_LEITURA;
    }

    fseek(dados, from, SEEK_CUR);
    if(fgetc (dados) == EOF){
        fclose(dados);
        return ERRO_DE_LEITURA;
    }
    
    fseek(dados, -1, SEEK_CUR);
    fread(linha, sizeof(char), tamTpl, dados); //Traz a tupla inteira do arquivo

    fclose(dados);
    return linha;
}
/////
void setTupla(tp_buffer *buffer,char *tupla, int tam, int pos) { //Coloca uma tupla de tamanho "tam" no buffer e na página "pos"
  int i = buffer[pos].position;
  for (; i < buffer[pos].position + tam; i++)
    buffer[pos].data[i] = *(tupla++);
}
//// insere uma tupla no buffer
int colocaTuplaBuffer(tp_buffer *buffer, int from, tp_table *campos, struct fs_objects objeto){//Define a página que será incluida uma nova tupla
    int i, found;
    char *tupla = getTupla(campos, objeto, from);
    if(tupla == ERRO_DE_LEITURA)  return ERRO_LEITURA_DADOS;

    int tam = tamTupla(campos, objeto);

    for(i = found = 0; !found && i < PAGES; i++) {//Procura pagina com espaço para a tupla.
        if(SIZE - buffer[i].position > tam) {// Se na pagina i do buffer tiver espaço para a tupla, coloca tupla.
            setTupla(buffer, tupla, tam, i);
            found = 1;
            buffer[i].position += tam; // Atualiza proxima posição vaga dentro da pagina.
            if(isDeleted(tupla)) {
                return ERRO_LEITURA_DADOS_DELETADOS;
            }
             buffer[i].nrec++;
        }
    }
    return found ? SUCCESS : ERRO_BUFFER_CHEIO;
}
////////

void cria_campo(int tam, int header, char *val, int x) {
  int i;
  char aux[30];
  if(header){
    for(i = 0; i <= 30 && val[i] != '\0'; i++) aux[i] = val[i];
    for(;i < 30;i++) aux[i] = ' ';
    aux[i] ='\0';
    printf("%s", aux);
    return;
  }
  for(i = 0; i < x; i++) printf(" ");
}

/* ----------------------------------------------------------------------------------------------
    Objetivo:   Utilizada para gravar as mudanças do buffer no disco.
    Parametros: Buffer (tp_buffer), dados da tabela (fs_objects), número de blocos e offset do bloco.
    Retorno:    1 para sucesso, 0 para falha.
   ---------------------------------------------------------------------------------------------*/
int writeBufferToDisk(tp_buffer *buffer, struct fs_objects *objeto) {
    int success = 1; // flag de sucesso porque sucesso deveria valer 1 não 0!
    char directory[LEN_DB_NAME_IO];
    strcpy(directory, connected.db_directory);
    strcat(directory, objeto->nArquivo);

    if(buffer==NULL){
        printf("ERROR: empty buffer\n");
        return 0;
    }

    FILE *dados = fopen(directory, "r+b");
    if (!dados) {
        printf("ERROR: Unable to open file for writing.\n");
        return 0;
    }
    
    fseek(dados, buffer->id *sizeof(tp_buffer), SEEK_SET);
    buffer->db = 0;
    buffer->pc = 0;
    fwrite(buffer, sizeof(tp_buffer), 1, dados);
    fclose(dados);

    return success;
}

static int isDeleted(char *linha){
    return linha[0]; //byte se foi deletado
}

void addColumn(column **colList, column *c){
    c->next = NULL;
    if(*colList == NULL) {
        *colList = c;
        return;
    }
    column *t = *colList;
    while(t->next != NULL) t = t->next;
    
    t->next = c;
}
