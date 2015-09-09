CFLAGS=-g -O2 
shell: parser.o analyser.o processing.o prompt.o
	gcc $(CFLAGS) parser.o analyser.o processing.o prompt.o -o shell -lfl
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
	rm *.o shell.tab.c lex.yy.c shell.tab.h
