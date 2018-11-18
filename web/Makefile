include ../user.cfg
include ../esp82xx/common.mf
PATH_TO_SRC = ../esp82xx/web/

.PHONY : all clean push
all : mfsmaker pushtodev execute_reflash page.mpfs 

CC = gcc
CFLAGS = $(OPTS)
LDLIBS = -lusb-1.0
PAGE_SCRIPT = $(foreach s, $(call uniq, $(notdir $(wildcard page/jquery*gz) menuinterface.js $(PAGE_SCRIPTS))), <script language="javascript" type="text/javascript" src="$(s)"></script>)

mfsmaker : $(PATH_TO_SRC)mfsmaker.c
pushtodev : $(PATH_TO_SRC)pushtodev.c
execute_reflash : $(PATH_TO_SRC)execute_reflash.c $(PATH_TO_SRC)md5.c

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
	./pushtodev $(IP) $(MFS_PAGE_OFFSET) page.mpfs

usbpush : pushtodev page.mpfs
	./pushtodev USB $(MFS_PAGE_OFFSET) page.mpfs

clean :
	$(RM) mfsmaker page.mpfs pushtodev execute_reflash tmp/*
