shell: parser.o analyser.o processing.o
	gcc -g -O2 parser.o analyser.o processing.o -o shell -lfl
parser.o: shell.y 
	bison -d shell.y
	gcc -g -O2 -c y.tab.c -o parser.o
analyser.o: shell.l
	flex shell.l
	gcc -g -O2 -c lex.yy.c -o analyser.o
processing.o:processing.c processing.h
	gcc -g -O2 -c processing.c -o processing.o
clean: 
	rm *.o
