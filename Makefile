all: dcheck dissemina

dissemina: dissemina.c
	gcc -Wall dissemina.c -o dissemina

dissemina-debug:
	gcc -Wall -g dissemina.c -o dissemina

dcheck: dcheck.c
	gcc -Wall dcheck.c -o dcheck

runtests: dissemina dcheck
	./dissemina &
	cat tests.dcheck | ./dcheck | grep FAILED || pkill dissemina
	pkill dissemina

