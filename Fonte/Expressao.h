#ifndef FEXPRESSAO
#define FEXPRESSAO 1

#define LOGICO 1
#define RELACIONAL 2
#define SINAL 3
#define ARITIMETICO 4
#define ABRE_PARENT 5
#define FECHA_PARENT 6
#define STRING 7
#define OBJETO 8
#define VALUE_NUMBER 9
#define BOOLEANO 10
#define NULLA 11

#ifndef FTYPES
  #include "types.h"
#endif

// Prototipos atualizados para refletir Expressao.c
char precedArit(int id);
int operador(int id);
void *converter(char tipo, char *valor);
void substitui(Lista *l, tupla *tuple);
Lista *resArit(Lista *l, tupla *tuple);
inf_where *opArit(Lista *l, tupla *tuple);
void aritPosfixa(Lista *l, tupla *tuple, Lista *novaExp);
column *buscaColuna(tupla *tuple, char *str);
Lista *relacoes(Lista *l);
char logPosfixa(Lista *l);
char opLog(Lista *l);
char precedLog(char *);

#endif // FEXPRESSAO
