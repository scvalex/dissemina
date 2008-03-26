all: dcheck dissemina

dissemina: dissemina.c dstdio.h drequest.h
	gcc -Wall dissemina.c -o dissemina

dissemina-debug: dissemina.c dstdio.h
	gcc -Wall -g dissemina.c -o dissemina

clean:
	rm dissemina

distclean: clean
realclean: clean

test: dissemina dcheck
	pkill dissemina || true
	./dissemina &
	cat tests.dcheck | ./dcheck -s | grep FAILED || true
	pkill dissemina || true

