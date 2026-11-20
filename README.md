# uffsdb
A simple educational DBMS

# dependencies
 1. GCC and Make
 2. Bison
 3. Flex
 4. Library readline

 - Linux (Debian/Ubuntu-based systems) or Windows via WSL:
 ```
 sudo apt install build-essential bison flex libreadline-dev
 ```

 - macOS (Homebrew):
 ```
 brew install bison flex readline
 ```

 - Windows: the project only runs through **WSL**. It uses `readline`, shell commands
   (`mkdir`, `rm`, `/dev/null`) and `pthread`, which do not exist natively on Windows
   (MinGW stops with `fatal error: readline/history.h: No such file or directory`).

# how to compile
 ```
 cd uffsdb/Fonte
 make          # normal build
 make debug    # build with [DEBUG] messages
 make clean    # removes the binary, the generated parser/lexer files and the data/ folder
 ```

 `make` and `make debug` produce the same `uffsdb` binary. To switch between the two modes,
 run `make clean` first.

# how to execute
 Always run from inside `Fonte/`, because the databases are stored in `./data`
 (running from another folder creates `data/` in the wrong place):
 ```
 cd uffsdb/Fonte
 ./uffsdb
 ```

# compiler
 uffsdb commands are interpreted using `bison` (`yacc`) and `flex` (`lex`).
 The main `Makefile` generates `interface/y.tab.c`, `interface/y.tab.h` and
 `interface/lex.yy.c` automatically; there is no need to compile the `interface` folder separately.
 You can edit the following files: `parser.h`, `parser.c`, `lex.l`, and `yacc.y`.
