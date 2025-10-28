#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "../memoryContext.h"
#ifndef FMACROS
   #include "../macros.h"
#endif
#ifndef FTYPES
   #include "../types.h"
#endif
#ifndef FUTILITY
   #include "../Utility.h"
#endif
#ifndef FMISC
   #include "../misc.h"
#endif
#ifndef FDATABASE
   #include "../database.h"
#endif
#ifndef FSQLCOMMANDS
   #include "../sqlcommands.h"
#endif
#ifndef FPARSER
   #include "parser.h"
#endif

/* Estrutura global que guarda as informações obtidas pelo yacc
 * na identificação dos tokens
 */
rc_insert GLOBAL_DATA;

/*
  Informações da operação select.
*/
inf_query QUERY;

/* Estrutura auxiliar do reconhecedor.
 */
rc_parser GLOBAL_PARSER;

void connect(char *nome) {
  int r = connectDB(nome);
	if (r == SUCCESS) {
    connected.db_name = uffsllocType(sizeof(char)*((strlen(nome)+1)), PERMANENT);
    strcpylower(connected.db_name, nome);
    connected.conn_active = 1;
    printf("You are now connected to database \"%s\" as user \"uffsdb\".\n", nome);
  }
  else {
  	printf("ERROR: Failed to establish connection with database named \"%s\". (Error code: %d)\n", nome, r);
  }
}

void invalidCommand(char *command) {
    printf("ERROR: Invalid command '%s'. Type \"help\" for help.\n", command);
}

void quit(int flag){
    write_history("data/history.txt");
    destroyMemoryContext();
    exit(flag);
};

void notConnected() {
    printf("ERROR: you are not connected to any database.\n");
}

void adcTabelaQuery(char *nome, char type){
  QUERY.tabela = uffslloc(strlen(nome)*sizeof(char));
  strcpy(QUERY.tabela,nome);
  QUERY.queryType = type;
}

int cmpSelect(void *a, void *b){
  return strcmp((char *)a,(char *)b);
}

void adcTokenWhere(char *token,int id){
  if(!QUERY.tok) QUERY.tok = novaLista(&cmpSelect);
  adcNodo(QUERY.tok, QUERY.tok->ult, (void *)novoTokenWhere(token,id));
}

void adcProjSelect(char *col){
  char *str = uffslloc(sizeof(char)*strlen(col));
  strcpy(str,col);
  if(!QUERY.proj) QUERY.proj = novaLista(NULL);
  adcNodo(QUERY.proj, QUERY.proj->ult, (void *)str);
}

void setObjName(char **nome) {
    if (GLOBAL_PARSER.mode != 0) {
        GLOBAL_DATA.objName = uffslloc(sizeof(char)*((strlen(*nome)+1)));
        strcpylower(GLOBAL_DATA.objName, *nome);
        GLOBAL_DATA.objName[strlen(*nome)] = '\0';
        GLOBAL_PARSER.step++;
    }
}

void setColumnInsert(char **nome) {
    GLOBAL_DATA.columnName = uffsRealloc(GLOBAL_DATA.columnName, (GLOBAL_PARSER.col_count+1)*sizeof(char *));

    GLOBAL_DATA.columnName[GLOBAL_PARSER.col_count] = uffslloc(sizeof(char)*(strlen(*nome)+1));
    strcpylower(GLOBAL_DATA.columnName[GLOBAL_PARSER.col_count], *nome);
    GLOBAL_DATA.columnName[GLOBAL_PARSER.col_count][strlen(*nome)] = '\0';

    GLOBAL_PARSER.col_count++;
}

void setValueInsert(char *nome, char type) {
    int i;
    GLOBAL_DATA.values  = uffsRealloc(GLOBAL_DATA.values, (GLOBAL_PARSER.val_count+1)*sizeof(char *));
    GLOBAL_DATA.type    = uffsRealloc(GLOBAL_DATA.type, (GLOBAL_PARSER.val_count+1)*sizeof(char));

    // Adiciona o valor no vetor de strings
    GLOBAL_DATA.values[GLOBAL_PARSER.val_count] = uffslloc(sizeof(char)*(strlen(nome)+1));
    if (type == 'I' || type == 'D') {
        strcpy(GLOBAL_DATA.values[GLOBAL_PARSER.val_count], nome);
        GLOBAL_DATA.values[GLOBAL_PARSER.val_count][strlen(nome)] = '\0';
    } else if (type == 'S') {
        for (i = 1; i < strlen(nome)-1; i++) {
            GLOBAL_DATA.values[GLOBAL_PARSER.val_count][i-1] = nome[i];
        }
        GLOBAL_DATA.values[GLOBAL_PARSER.val_count][strlen(nome)-2] = '\0';
    }

    GLOBAL_DATA.type[GLOBAL_PARSER.val_count] = type;

    GLOBAL_PARSER.val_count++;
}

void setColumnCreate(char **nome) {
    GLOBAL_DATA.columnName  = uffsRealloc(GLOBAL_DATA.columnName, (GLOBAL_PARSER.col_count+1)*sizeof(char *));
    GLOBAL_DATA.attribute   = uffsRealloc(GLOBAL_DATA.attribute, (GLOBAL_PARSER.col_count+1)*sizeof(int));
    GLOBAL_DATA.fkColumn    = uffsRealloc(GLOBAL_DATA.fkColumn, (GLOBAL_PARSER.col_count+1)*sizeof(char *));
    GLOBAL_DATA.fkTable     = uffsRealloc(GLOBAL_DATA.fkTable, (GLOBAL_PARSER.col_count+1)*sizeof(char *));
    GLOBAL_DATA.values      = uffsRealloc(GLOBAL_DATA.values, (GLOBAL_PARSER.col_count+1)*sizeof(char *));
    GLOBAL_DATA.type        = uffsRealloc(GLOBAL_DATA.type, (GLOBAL_PARSER.col_count+1)*sizeof(char *));

    GLOBAL_DATA.values[GLOBAL_PARSER.col_count] = uffslloc(sizeof(char));
    GLOBAL_DATA.fkTable[GLOBAL_PARSER.col_count] = uffslloc(sizeof(char));
    GLOBAL_DATA.fkColumn[GLOBAL_PARSER.col_count] = uffslloc(sizeof(char));
    GLOBAL_DATA.columnName[GLOBAL_PARSER.col_count] = uffslloc(sizeof(char)*(strlen(*nome)+1));

    strcpylower(GLOBAL_DATA.columnName[GLOBAL_PARSER.col_count], *nome);

    GLOBAL_DATA.columnName[GLOBAL_PARSER.col_count][strlen(*nome)] = '\0';
    GLOBAL_DATA.type[GLOBAL_PARSER.col_count] = 0;
    GLOBAL_DATA.attribute[GLOBAL_PARSER.col_count] = NPK;

    GLOBAL_PARSER.col_count++;
    GLOBAL_PARSER.step = 2;
}

void setColumnTypeCreate(char type){
    GLOBAL_DATA.type[GLOBAL_PARSER.col_count-1] = type;
    GLOBAL_PARSER.step++;
}

void setColumnSizeCreate(char *size){
  GLOBAL_DATA.values[GLOBAL_PARSER.col_count-1] = uffsRealloc(GLOBAL_DATA.values[GLOBAL_PARSER.col_count-1], sizeof(char)*(strlen(size)+1));
  strcpy(GLOBAL_DATA.values[GLOBAL_PARSER.col_count-1], size);
  GLOBAL_DATA.values[GLOBAL_PARSER.col_count-1][strlen(size)] = '\0';
}

void setColumnPKCreate() {
    GLOBAL_DATA.attribute[GLOBAL_PARSER.col_count-1] = PK;
}

void setColumnBtreeCreate(char **nome) {
    GLOBAL_DATA.columnName = uffsRealloc(GLOBAL_DATA.columnName, (GLOBAL_PARSER.col_count+1)*sizeof(char*));
    GLOBAL_DATA.columnName[GLOBAL_PARSER.col_count] = uffslloc(sizeof(char)*(strlen(*nome)+1));
    strcpylower(GLOBAL_DATA.columnName[GLOBAL_PARSER.col_count], *nome);
}

void setColumnFKTableCreate(char **nome) {
    GLOBAL_DATA.fkTable[GLOBAL_PARSER.col_count-1] = uffsRealloc(GLOBAL_DATA.fkTable[GLOBAL_PARSER.col_count-1], sizeof(char)*(strlen(*nome)+1));
    strcpylower(GLOBAL_DATA.fkTable[GLOBAL_PARSER.col_count-1], *nome);
    GLOBAL_DATA.fkTable[GLOBAL_PARSER.col_count-1][strlen(*nome)] = '\0';
    GLOBAL_DATA.attribute[GLOBAL_PARSER.col_count-1] = FK;
    GLOBAL_PARSER.step++;
}

void setColumnFKColumnCreate(char **nome) {
    GLOBAL_DATA.fkColumn[GLOBAL_PARSER.col_count-1] = uffsRealloc(GLOBAL_DATA.fkColumn[GLOBAL_PARSER.col_count-1], sizeof(char)*(strlen(*nome)+1));
    strcpylower(GLOBAL_DATA.fkColumn[GLOBAL_PARSER.col_count-1], *nome);
    GLOBAL_DATA.fkColumn[GLOBAL_PARSER.col_count-1][strlen(*nome)] = '\0';
    GLOBAL_PARSER.step++;
}

void limparLista(Lista *l){
  Nodo *k = l->prim,*j;
  while(k){
    j = k->prox;
    rmvNodoPtr(l,k);
    k = j;
  }
  l->prim = l->ult = NULL;
}

void resetQuery() {
    if(getMode() == OP_SELECT || getMode() == OP_DELETE) {
        if(QUERY.tabela) {
            QUERY.tabela = NULL;
        }
        if(QUERY.tok) limparLista(QUERY.tok);
        QUERY.tok = NULL;
        if(QUERY.proj) limparLista(QUERY.proj);
        QUERY.proj = NULL;
    }
}

void clearGlobalStructs() {
    resetQuery();
    if (GLOBAL_DATA.objName) {
        GLOBAL_DATA.objName = NULL;
    }

    GLOBAL_DATA.columnName = NULL;
    GLOBAL_DATA.values = NULL;
    GLOBAL_DATA.fkTable = NULL;
    GLOBAL_DATA.fkColumn = NULL;

    GLOBAL_DATA.type = (char *)uffslloc(sizeof(char));

    GLOBAL_DATA.attribute = (int *)uffslloc(sizeof(int));

    yylex_destroy();

    GLOBAL_DATA.N = 0;

    GLOBAL_PARSER.mode              = 0;
    GLOBAL_PARSER.parentesis        = 0;
    GLOBAL_PARSER.noerror           = 1;
    GLOBAL_PARSER.col_count         = 0;
    GLOBAL_PARSER.val_count         = 0;
    GLOBAL_PARSER.step              = 0;
}

void setMode(char mode) {
    GLOBAL_PARSER.mode = mode;
    GLOBAL_PARSER.step++;
}

char getMode() {
    return GLOBAL_PARSER.mode;
}

int interface() {
    pthread_t pth;
    pthread_create(&pth, NULL, (void*)clearGlobalStructs, NULL);
    pthread_join(pth, NULL);

    char prompt[LEN_DB_NAME + 4]; // 3 para "=# " +1 para \0
    Lista *resultado;
    connect("uffsdb"); // conecta automaticamente no banco padrão
    QUERY.tok = QUERY.proj = NULL;
    historyInit();
    while(1){
        if (!connected.conn_active) {
            snprintf(prompt, 3, "> ");
        } else {
            snprintf(prompt, sizeof(prompt), "%s=# ", connected.db_name);
        }
        fflush(stdout);
        char *input = readline(prompt); 
        if(!input) break;
        
        if(!(*input)) continue;
        getComando(input);
        free(input);
        
        if (GLOBAL_PARSER.noerror) {
            if (GLOBAL_PARSER.mode != 0) {
                if (!connected.conn_active) {
                    notConnected();
                } else {
                    switch(GLOBAL_PARSER.mode) {
                        case OP_INSERT:
                            if (GLOBAL_DATA.N > 0) {
                                insert(&GLOBAL_DATA);
                            }
                            else
                                printf("WARNING: Nothing to be inserted. Command ignored.\n");
                            break;
                        case OP_SELECT:
                            resultado = handleTableOperation(&QUERY, 's');
                            if (resultado) {
                                printConsulta(QUERY.proj, resultado);
                                resultado = NULL;
                            }
                            break;
                        case OP_DELETE:
                            resultado = handleTableOperation(&QUERY, 'd');
                            if (resultado && afterTrigger(resultado, &QUERY)) {
                                op_delete(resultado, QUERY.tabela);
                                resultado = NULL;
                            }
                            break;
                        case OP_CREATE_TABLE:
                            createTable(&GLOBAL_DATA);
                            break;
                        case OP_CREATE_DATABASE:
                            createDB(GLOBAL_DATA.objName);
                            break;
                        case OP_DROP_TABLE:
                            excluirTabela(GLOBAL_DATA.objName);
                            break;
                        case OP_DROP_DATABASE:
                            dropDatabase(GLOBAL_DATA.objName);
                            break;
                        case OP_CREATE_INDEX:
                            createIndex(&GLOBAL_DATA);
                            break;
                        default: break;
                    }

                }
            }
        } else {
            GLOBAL_PARSER.consoleFlag = 1;
            switch(GLOBAL_PARSER.mode) {
                case OP_CREATE_DATABASE:
                case OP_DROP_DATABASE:
                case OP_CREATE_TABLE:
                case OP_DROP_TABLE:
                case OP_SELECT:
                case OP_INSERT:
                case OP_CREATE_INDEX:
                    if (GLOBAL_PARSER.step == 1) {
                        GLOBAL_PARSER.consoleFlag = 0;
                        printf("Expected object name.\n");
                    }
                break;

                default: break;
            }

            if (GLOBAL_PARSER.mode == OP_CREATE_TABLE) {
                if (GLOBAL_PARSER.step == 2) {
                    printf("Column not specified correctly.\n");
                    GLOBAL_PARSER.consoleFlag = 0;
                }
            } else if (GLOBAL_PARSER.mode == OP_INSERT) {
                if (GLOBAL_PARSER.step == 2) {
                    if(GLOBAL_PARSER.parentesis == 1 && GLOBAL_PARSER.val_count == 1){
                        printf("ERROR: Invalid syntax after \"VALUES\". Ensure that the values are correct.\n");
                    }else if(GLOBAL_PARSER.parentesis == 0){
                        printf("Invalid syntax.\n");
                    }else if(GLOBAL_PARSER.val_count == 0){
                        printf("Invalid syntax.\n");
                    }else{
                        printf("Expected token \"VALUES\" after object name.\n");
                    }
                    
                    GLOBAL_PARSER.consoleFlag = 0;
                }
            }

            printf("ERROR: syntax error.\n");
            GLOBAL_PARSER.noerror = 1;
        }

        if (GLOBAL_PARSER.mode != 0) {
            pthread_create(&pth, NULL, (void*)clearGlobalStructs, NULL);
            pthread_join(pth, NULL);
        }
    
        uffsFree(TEMPORARY);
    }
    return 0;
}

void yyerror(char *s, ...) {
  GLOBAL_PARSER.noerror = 0;
  /*extern yylineno;

  va_list ap;
  va_start(ap, s);

  fprintf(stderr, "%d: error: ", yylineno);
  vfprintf(stderr, s, ap);
  fprintf(stderr, "\n");
  */
}
