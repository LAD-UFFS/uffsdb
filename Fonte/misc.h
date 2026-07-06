#define FMISC 1
//
#ifndef FTYPES // garante que types.h não seja reincluída
  #include "types.h"
#endif

void cria_campo(int , int , char *, int );

int cabecalho(tp_table *, int );
int drawline(tupla *t, tp_table *s, struct fs_objects objeto);
void help();
void implement();
void contr();
int objcmp(char *, char *);
void strcpylower(char *, const char *);
void strncpylower(char *, char *, int);
/* ----------------------------------------------------------------------------------------------
    Objetivo:   Verifica se o nome da tabela 'nomeTabela' está nos primeiros bytes de 'linha'
    Parametros: Nome da tabela, char linha.
    Retorno:    INT(1 - Está contido, 0 - Não está)
   ---------------------------------------------------------------------------------------------*/
int TrocaArquivosObj(char *nomeTabela, char *linha);
///
int pot10(int );

int strtam(char []);

int convertI(char []);

double get_decimal(char []);

double get_inteiro(char []);

double convertD(char c[]);

void clear();

/*
* printa o histórico de comandos
*/
void printHistory();

/*
* limpa o histórico de comandos
*/
void deleteHistory();