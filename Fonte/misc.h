#define FMISC 1
//
#ifndef FTYPES // garante que types.h não seja reincluída
  #include "types.h"
#endif

int cabecalho(tp_table *, int );
int drawline(tp_buffer *, tp_table *, struct fs_objects, int , int);
void help();
void implement();
void contr();
/*
 * ObjCmp - Compara duas strings lexicograficamente.
 *
 * Converte cada caractere para minúsculo e, ao encontrar o primeiro par
 * de caracteres diferentes, retorna (a - b).
 *
 * Caso todos os caracteres avaliados sejam iguais até o fim de uma das
 * strings, retorna a diferença entre os caracteres atuais, o que pode ser
 * positivo, negativo ou zero.
 *
 * Retorna: Valor negativo se obj < str; zero se forem equivalentes; valor positivo se obj > str.
 */
int objcmp(char *, char *);
void strcpylower(char *, char *);
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
