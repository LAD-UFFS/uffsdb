#pragma once

#include "Utility.h"
#include "int.h"
#include <stdint.h>
#include "macros.h"

#define FTYPES 1 // flag para identificar se types.h já foi incluída

// isso é um registro da TABELA
struct fs_objects { // Estrutura usada para carregar fs_objects.dat
    char nome[TAMANHO_NOME_TABELA];      // Nome da tabela.
    int cod;                             // Código da tabela.
    char nArquivo[TAMANHO_NOME_ARQUIVO]; // Nome do arquivo onde estão armazenados os dados da tabela.
    int qtdCampos;                       // Quantidade de campos da tabela.
    int qtdIndice;						 // Quantidade de índices da tabela.
    int16_t lastBuffer;
};

typedef struct tp_table{ // Estrutura usada para carregar fs_schema.dat
    int id;                       // Código da tabela.                   2bytes
    char nome[TAMANHO_NOME_CAMPO];  // Nome do Campo.                    40bytes
    char tipo;                      // Tipo do Campo.                     1bytes
    int tam;                        // Tamanho do Campo.                  4bytes
    int chave;                      // Tipo da chave                      4bytes
    char tabelaApt[TAMANHO_NOME_TABELA]; //Nome da Tabela Apontada        20bytes
    char attApt[TAMANHO_NOME_CAMPO];    //Nome do Atributo Apontado       40bytes
    struct tp_table *next;          // Encadeamento para o próximo campo.
}tp_table;

typedef struct column{ // Estrutura utilizada para inserir em uma tabela, excluir uma tupla e retornar valores de uma página.
    char tipoCampo;                     // Tipo do Campo.
    char nomeCampo[TAMANHO_NOME_CAMPO]; //Nome do Campo.
    char *valorCampo;                   // Valor do Campo.
    struct column *next;                // Encadeamento para o próximo campo.
}column;

typedef struct tupla {
    unsigned int offset;
    uint ncols; // Número de colunas na tupla.
    uint bufferPage; // Número do bloco (no disco) onde essa tupla específica está dentro do arquivo da tabela (bloco 0, bloco 1, etc. da tabela)
    column *column;
}tupla;

typedef struct {
    tupla *tuplas;
    int nrec;
} PageResult;

typedef struct table{ // Estrutura utilizada para criar uma tabela.
    char nome[TAMANHO_NOME_TABELA]; // Nome da tabela.
    tp_table *esquema;              // Esquema de campos da tabela.
}table;

// // mudamos o nome de "tp_buffer" para "tp_pagina":
// // também tiramos pc e db da página, pois o professor disse que eles não eram para ir para disco, mas sim ficar só na memória
// typedef struct tp_pagina{ // Estrutura utilizada para armazenar o buffer.
//     unsigned int id;        // posição do bloco no arquivo em relação aos outros blocos [0,1,2,...[
//     unsigned int nrec;       //Número de registros armazenados na página.
//     uint32_t position;   // Número da quantidade de registro que a página ainda pode receber;
//     // TODO: Separar a struct o que precisar ser persistida e o que fica na memória
//     // unsigned char db;        //Dirty bit
//     // unsigned char pc;        //Pin counter
//     char data[SIZE];         // Dados
// }tp_pagina;

typedef struct rc_insert {
    char    *objName;           // Nome do objeto (tabela, banco de dados, etc...)
    char   **columnName;        // Colunas da tabela
    char   **values;            // Valores da inserção ou tamanho das strings na criação
    int      N;                 // Número de colunas de valores
    char    *type;              // Tipo do dado da inserção ou criação de tabela
    int     *attribute;         // Utilizado na criação (NPK, PK,FK)
    char   **fkTable;           // Recebe o nome da tabela FK
    char   **fkColumn;          // Recebe o nome da coluna FK
}rc_insert;

typedef struct inf_where{
  int id;
  void *token;
}inf_where;

typedef struct  inf_query {
  char *tabela;     // Nome da tabela
  int tamTokens;    // Número de tokens na cláusula WHERE
  Lista *tok;       // Lista de condições WHERE
  Lista *proj;      // Colunas recuperadas (NULL para DELETE)
  Lista *values;    // Lista de valores para UPDATE
  char queryType;   // 'S' for SELECT, 'D' for DELETE, 'U' for UPDATE
} inf_query;

typedef struct rc_parser {
    int         mode;           // Modo de operação (definido em /interface/parser.h)
    int         parentesis;     // Contador de parenteses abertos
    int         step;           // Passo atual (token)
    int         noerror;        // Nenhum erro encontrado na identificação dos tokens
    int         col_count;      // Contador de colunas
    int         val_count;      // Contador de valores
    int         consoleFlag;   // Auxiliar para não imprimir duas vezes nome=#
}rc_parser;

typedef struct data_base{
	char 		valid;
	char 		db_name[LEN_DB_NAME_IO];
	char 		db_directory[LEN_DB_NAME_IO];
}data_base;

typedef struct db_connected {
	char db_directory[LEN_DB_NAME_IO];
    char *db_name;
    int conn_active;
}db_connected;

// Sessão para fs do sistema 

typedef struct fs_constraint {
    uint tableId; // ID da tabela
    uint idFKtable; //  ID da Tabela referenciada
    char constraintName[TAMANHO_NOME_CONSTRAINT]; // Nome da restrição
    char columnName[TAMANHO_NOME_CAMPO]; // Nome da coluna
    char columnApt[TAMANHO_NOME_CAMPO]; // Nome da coluna referenciada
    byte constraintType; // Tipo de restrição (PK, FK, NPK)
    byte deltype; // Tipo de deleção (CASCADE, SET NULL, RESTRICT)
} fs_constraint;

typedef enum {
   TEMPORARY,
   PERMANENT
} MemoryContextType;

typedef struct MemoryContext {
   MemoryContextType type;
   uint used;
   struct MemoryContext *next;
    char memoryPool[MEMORY_CONTEXT_SIZE];
} MemoryContext;

typedef struct MemoryContextRoot {
   struct MemoryContext *temporary, *permanent;
} MemoryContextRoot;


typedef struct {
    size_t size;
    char data[FLEXIBLE_ARRAY];
} uffs_mem_header;

// Union's utilizados na conversão de variáveis do tipo inteiro e double.

union c_double{

    double dnum;
    char double_cnum[sizeof(double)];
};

union c_int{

    int  num;
    char cnum[sizeof(int)];
};


// structs que criamos:

// #define QTD_PAGINAS_BUFFER_POOL 50; // vimos que já tem uma constante que diz isso na macros.h, com nome PAGES
#define TAM_PAGINA_BF 400

typedef struct um_slot_do_header_do_buffer_pool {
    int id_tabela; // qual tabela ocupa este slot (-1 = slot livre) // = fs_objects.cod (ou seja: o id/código da tabela)
    int bloco_da_tabela; // qual bloco da tabela "id_tabela" está neste slot
    unsigned char db; //Dirty bit
    unsigned char pc; //Pin counter
    char filename[LEN_DB_NAME_IO]; // pois sem o filename não tem como descorbrir o fs_objects da tabela pelo bp (buffer pool)
                                   // precisamos guardar o caminho do arquivo pra saber onde gravar quando quando o bloco sair do buffer pool e ir para o disco
} um_slot_do_header_do_buffer_pool;

typedef struct tp_pagina{ // Estrutura utilizada para armazenar cada página.
    unsigned int id; // posição do bloco no arquivo em relação aos outros blocos [0,1,2,...[
    unsigned int nrec; //Número de registros armazenados na página.
    uint32_t position; // Tamanho total dos registro que tem na página
    char data[SIZE]; // são os dados da página (as tuplas) -> SIZE = 1024 bytes de dados
} tp_pagina;

typedef struct buffer_pool {//estrutura do buffer poll
	int qtd_paginas_total;
	int qtd_paginas_ocupadas;
	int qtd_paginas_desocupadas;
	um_slot_do_header_do_buffer_pool header[PAGES]; // esse vai ser o header do buffer pool: cada índice desse vetor vai ter informações sobre o índice correspondente vetor da área de dados (vetor "páginas") 
	tp_pagina paginas[PAGES]; // área onde vão estar todas as páginas que estão no BP
} buffer_pool;


/************************************************************************************************
**************************************  VARIAVEIS GLOBAIS  **************************************/

extern db_connected connected;

extern buffer_pool bp; // coloquei aqui pois é uma variável global

/************************************************************************************************
 ************************************************************************************************/
