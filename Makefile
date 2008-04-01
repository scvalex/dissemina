CFLAGS = -g -O2 -Wall
LDFLAGS = 

CC = gcc
RM = rm -f
FIND = find

QUIET_CC = @echo '    ' CC $@;
QUIET_LINK = @echo '    ' LINK $@;

all: dcheck dissemina

dissemina: dissemina.o
	$(QUIET_LINK)$(CC) $(LDFLAGS) dissemina.o -o dissemina

dissemina.o: dissemina.c dstdio.h dstring.h dhandlers.h drequest.h 
	$(QUIET_CC)$(CC) -o dissemina.o -c $(CFLAGS) dissemina.c

configure: configure.ac
	autoconf

clean:
	$(RM) dissemina
	$(RM) *.o
	$(RM) -r autom4te.cache config.log configure

distclean: clean
realclean: clean

%.o: %.c
	$(QUIET_CC)$(CC) -o $*.o -c $(CFLAGS) $<

tags:
	$(RM) tags
	$(FIND) . -name '*.[hc]' -print | xargs etags -a

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

