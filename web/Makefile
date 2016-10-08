include ../user.cfg
include ../esp82xx/common.mf

.PHONY : all clean push
all : execute_reflash push

CC = gcc
CFLAGS = $(OPTS)
LDLIBS = -lusb-1.0
PAGE_SCRIPT = $(foreach s, $(call uniq, $(notdir $(wildcard page/jquery*gz) menuinterface.js $(PAGE_SCRIPTS))), <script language="javascript" type="text/javascript" src="$(s)"></script>)

mfsmaker : mfsmaker.c
pushtodev : pushtodev.c
execute_reflash : execute_reflash.c md5.c

mfsmaker pushtodev execute_reflash :
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lusb-1.0

page.mpfs : mfsmaker $(wildcard page/*) Makefile
	$(RM) -r tmp
	$(CP) -r page tmp
	# Inception level over 9000 for poor man's template substitution
	$(info bla$ ${\n} blubb)
	$(foreach p, $(filter PAGE_%,$(.VARIABLES)) $(filter PROJECT_%,$(.VARIABLES)) VERSSTR, \
		sed -i "s/{{$(p)}}/$$(echo -n '$($(p))' | sed -e 's/[\/&"]/\\&/g')/" tmp/index.html && \
	) true
	./mfsmaker tmp page.mpfs

push : pushtodev page.mpfs
	./pushtodev $(IP) $(PAGE_OFFSET) page.mpfs

usbpush : pushtodev page.mpfs
	./pushtodev USB $(PAGE_OFFSET) page.mpfs

clean :
	$(RM) mfsmaker page.mpfs pushtodev execute_reflash tmp/*
