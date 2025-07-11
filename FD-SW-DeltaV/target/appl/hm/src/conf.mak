#target/appl/hm/src

SOURCE := hm.c hm_if.c hm_task.c

LintDirArg := $(addprefix +libm$(LPAREN)$(_CURDIR)\,$(addsuffix $(RPAREN),$(SOURCE)))

SOURCE+= hm_dma.c