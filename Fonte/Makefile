LDLIBS = -lfl -lm -pthread -lreadline
CFLAGS = -Wall -g -Iinclude

all: uffsdb

debug: interface/y.tab.c interface/y.tab.h interface/lex.yy.c *.c *.h
	gcc -DDEBUG *.c $(CFLAGS) interface/*.c $(LDLIBS) -o uffsdb

uffsdb: interface/y.tab.c interface/y.tab.h interface/lex.yy.c *.c *.h
	gcc *.c $(CFLAGS) interface/*.c $(LDLIBS) -o uffsdb

interface/y.tab.c interface/y.tab.h: interface/lex.l interface/yacc.y
	lex -o interface/lex.yy.c interface/lex.l
	yacc -d -o interface/y.tab.c interface/yacc.y

clean:
	rm uffsdb -f
	rm data/ -RIf
	rm interface/y.tab.c interface/y.tab.h interface/lex.yy.c

