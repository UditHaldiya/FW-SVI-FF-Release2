#target/appl/fbif/src
SOURCE:= aifb.c aofb.c arfb.c csfb.c difb.c dofb.c \
    fbif_dsc.c instance.c isfb.c maifb.c osfb.c \
    pidfb.c ptb.c resb.c spec_ds.c

ISUBDIR:= ..\inc

LintDirArg := $(addprefix +libm$(LPAREN)$(_CURDIR)\,$(addsuffix $(RPAREN),$(SOURCE)))
