all: dcheck dissemina

dissemina: dissemina.c dstdio.h drequest.h
	gcc -Wall dissemina.c -o dissemina

dissemina-debug: dissemina.c dstdio.h
	gcc -Wall -g dissemina.c -o dissemina

clean:
	rm dissemina

distclean: clean
realclean: clean

tags: *.c *.h
	ctags *.c *.h

test: dissemina
	pkill dissemina || true
	./dissemina &
	sleep 1
	cat tests.dcheck | ./dcheck -s | grep FAILED || true
	pkill dissemina || true

testv: dissemina
	pkill dissemina || true
	./dissemina &
	sleep 1
	cat tests.dcheck | ./dcheck -v
	pkill dissemina || true

