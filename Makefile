DRC_GENERATED_FILES = $(patsubst Resources/%.in,%,$(wildcard Resources/*.in))

CFLAGS = -g -O2 -Wall
VERSION = 0.0.2

CC = gcc
RM = rm -f
FIND = find

QUIET_CC = @echo '    ' CC $@;
QUIET_LINK = @echo '    ' LINK $@;
QUIET_GEN = @echo '    ' GEN $@;
HIDE = @echo;

.PHONY: all
all: dcheck dissemina

dissemina: dissemina.o commonpages.o dnet.o dstdio.o
	$(QUIET_LINK)$(CC) $^ -o $@

dissemina.o: dissemina.c dstdio.h dstring.h dhandlers.h drequest.h lex.yy.c
	$(QUIET_CC)$(CC) -c $(CFLAGS) -DVERSION=\"$(VERSION)\" dissemina.c -o $@

%.o: %.c
	$(QUIET_CC)$(CC) -o $*.o -c $(CFLAGS) $<

commonpages.c: Resources/commonpages.c.in
	$(QUIET_GEN)./drc $< > $@

lex.yy.c: parser.l
	$(QUIET_GEN)flex $<

tags:
	$(RM) tags
	$(FIND) . -name '*.[hc]' -print | xargs etags -a

FILES: *.[ch]
	@echo '    ' RM $@; $(RM) $@
	$(QUIET_GEN) echo "What each file is" > $@ && \
	echo "" >> $@ && \
	for f in *.[ch]; do\
		echo "/" >> $@;\
		head -10 $$f | grep -E '^[^/]\*([^/]|$$)' | cut -c4- | sed 's/\t/  /g' | awk '{ print "| ",$$0 } ' >> $@;\
		echo "\\" >> $@;\
		echo >> $@;\
	done &&\
	echo >> $@ &&\
	echo "Generated by Makefile at "$$(ddate "+%e %B %Y") >> $@

.PHONY: clean
clean:
	$(RM) dissemina
	$(RM) *.o

.PHONY: distclean
distclean: clean
	$(RM) $(DRC_GENERATED_FILES)
	$(RM) *.yy.c *.tab.[c,h]

.PHONY: realclean
realclean: clean

.PHONY: test
.IGNORE: test
test: dissemina
	pkill dissemina 
	./dissemina &
	sleep 1
	cat tests.dcheck | ./dcheck -s | grep FAILED 
	pkill dissemina 

.IGNORE: testv
testv: dissemina
	pkill dissemina
	./dissemina &
	sleep 1
	cat tests.dcheck | ./dcheck -v
	pkill dissemina

