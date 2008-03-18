dcheck: dcheck.c
	gcc -Wall dcheck.c -o dcheck

dissemina: dissemina.c
	gcc -Wall dissemina.c -o dissemina

dissemina-debug:
	gcc -Wall -g dissemina.c -o dissemina

