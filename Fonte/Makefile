# CC, LEX e YACC ja tem valor padrao no make (cc, lex, yacc), por isso "=" e nao "?=".
# Para trocar: make LEX=lex YACC=yacc
CC      = gcc
LEX     = flex
YACC    = bison -y
CFLAGS  = -Wall -g -Iinclude
LDLIBS  = -lm -pthread -lreadline

SRCS      = $(wildcard *.c) interface/parser.c interface/commandHandler.c
HDRS      = $(wildcard *.h) $(wildcard interface/*.h)
GENERATED = interface/y.tab.c interface/y.tab.h interface/lex.yy.c

all: uffsdb

uffsdb: $(SRCS) $(HDRS) $(GENERATED)
	$(CC) $(CFLAGS) $(SRCS) interface/y.tab.c interface/lex.yy.c $(LDLIBS) -o $@

debug: $(SRCS) $(HDRS) $(GENERATED)
	$(CC) -DDEBUG $(CFLAGS) $(SRCS) interface/y.tab.c interface/lex.yy.c $(LDLIBS) -o uffsdb

interface/y.tab.c interface/y.tab.h: interface/yacc.y
	$(YACC) -d -o interface/y.tab.c interface/yacc.y

interface/lex.yy.c: interface/lex.l interface/y.tab.h
	$(LEX) -o interface/lex.yy.c interface/lex.l

clean:
	rm -f uffsdb $(GENERATED)
	rm -rf data/

.PHONY: all debug clean
