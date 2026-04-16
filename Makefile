CC     = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -g

.PHONY: all clean

all: P4

P4: P4.o parser.o semantics.o codegen.o tree.o lexer.o
	$(CC) $(CFLAGS) -o P4 P4.o parser.o semantics.o codegen.o tree.o lexer.o

P4.o: P4.c parser.h lexer.h token.h tree.h semantics.h codegen.h
	$(CC) $(CFLAGS) -c P4.c

parser.o: parser.c parser.h tree.h lexer.h token.h
	$(CC) $(CFLAGS) -c parser.c

semantics.o: semantics.c semantics.h tree.h token.h
	$(CC) $(CFLAGS) -c semantics.c

codegen.o: codegen.c codegen.h tree.h token.h
	$(CC) $(CFLAGS) -c codegen.c

tree.o: tree.c tree.h token.h
	$(CC) $(CFLAGS) -c tree.c

lexer.o: lexer.c lexer.h token.h
	$(CC) $(CFLAGS) -c lexer.c

clean:
	rm -f *.o P4
