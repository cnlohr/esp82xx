include ../user.cfg
include ../esp82xx/common.mf

.PHONY : all clean push
all : execute_reflash push

CC = gcc
CFLAGS = $(OPTS)

mfsmaker : mfsmaker.c
pushtodev : pushtodev.c
execute_reflash : execute_reflash.c md5.c

mfsmaker pushtodev execute_reflash : 
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lusb-1.0

page.mpfs : mfsmaker $(wildcard page/*) Makefile
	$(RM) -r tmp
	$(CP) -r page tmp
	$(info $(PROJECT_URL))
	sed -i 's/{{PAGE_TITLE}}/$(subst /,\/,$(PAGE_TITLE))/' tmp/index.html
	sed -i 's/{{PAGE_SCRIPT}}/$(subst /,\/,$(PAGE_SCRIPT))/' tmp/index.html
	sed -i 's/{{PAGE_HEADING}}/$(subst /,\/,$(PAGE_HEADING))/' tmp/index.html
	sed -i 's/{{PAGE_INFO}}/$(subst /,\/,$(PAGE_INFO))/' tmp/index.html
	sed -i 's/{{VERSSTR}}/$(subst ",,$(VERSSTR))/' tmp/index.html
	sed -i 's/{{PROJECT_URL}}/$(subst ",,$(subst /,\/,$(PROJECT_URL)))/' tmp/index.html
	sed -i 's/{{PROJECT_NAME}}/$(subst ",,$(PROJECT_NAME))/' tmp/index.html
	sed -i 's/{{PAGE_JQUERYFL}}/$(subst page/,,$(wildcard page/jquery*gz))/' tmp/index.html
	./mfsmaker tmp page.mpfs

push : pushtodev page.mpfs
	./pushtodev $(IP) $(PAGE_OFFSET) page.mpfs

clean :
	$(RM) mfsmaker page.mpfs pushtodev execute_reflash tmp/*

