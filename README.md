# uffsdb
A simple educational DBMS

# dependencies
 1. Bison
 2. Flex
 3. Library readline

 - For Ubuntu/Debian-based systems, run:
 ```
 sudo apt install bison
 sudo apt install flex
 sudo apt-get install libreadline-dev
```

# how to compile
 uffsdb/Fonte/make

# how to execute
 `uffsdb/Fonte/./uffsdb`
 
# compiler
 uffsdb commands are interpreted using `yacc` and `lex`.
 In the `interface` folder type `make` to compile both.
 You can edit the following files: `parser.h`, `parser.c`, `lex.l`, and `yacc.y`.
