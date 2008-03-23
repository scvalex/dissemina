all: dcheck dissemina

dissemina: dissemina.c dstdio.h drequest.h
	gcc -Wall dissemina.c -o dissemina

dissemina-debug: dissemina.c dstdio.h
	gcc -Wall -g dissemina.c -o dissemina

dcheck: dcheck.c dstdio.h dnetio.h
	gcc -Wall dcheck.c -o dcheck

runtests: dissemina dcheck
	pkill dissemina || true
	./dissemina &
	sleep 1
	cat tests.dcheck | ./dcheck | grep FAILED || true
	pkill dissemina || true

