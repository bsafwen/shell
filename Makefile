CFLAGS=-g -O2 -Werror -Wfatal-errors 
OBJECTS=parser.o analyser.o processing.o prompt.o
LINK=-lfl
shell: $(OBJECTS)
	gcc $(CFLAGS) $(OBJECTS) $(LINK) -o shell
parser.o: shell.y 
	bison -d shell.y
	gcc $(CFLAGS) -c shell.tab.c -o parser.o
analyser.o: shell.l
	flex shell.l
	gcc $(CFLAGS) -c lex.yy.c -o analyser.o
processing.o:processing.c processing.h
	gcc $(CFLAGS) -c processing.c -o processing.o
prompt.o: prompt.c prompt.h
	gcc $(CFLAGS) -c prompt.c -o prompt.o
clean: 
	rm -f $(OBJECTS) shell.tab.c lex.yy.c shell.tab.h
