CFLAGS = -g -O2 -Wall
VERSION = 0.0.1

CC = gcc
RM = rm -f
FIND = find

QUIET_CC = @echo '    ' CC $@;
QUIET_LINK = @echo '    ' LINK $@;
QUIET_GEN = @echo '    ' GEN $@;

all: dcheck dissemina

dissemina: dissemina.c dstdio.h dstring.h dhandlers.h drequest.h
	$(QUIET_CC)$(CC) -o dissemina $(CFLAGS) -DVERSION=\"$(VERSION)\" dissemina.c

%.o: %.c
	$(QUIET_CC)$(CC) -o $*.o -c $(CFLAGS) $<

tags:
	$(RM) tags
	$(FIND) . -name '*.[hc]' -print | xargs etags -a

clean:
	$(RM) dissemina
	$(RM) *.o

distclean: clean
realclean: clean

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

