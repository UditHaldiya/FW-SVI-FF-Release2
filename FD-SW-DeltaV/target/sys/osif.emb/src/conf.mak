#target/sys/osif.emb/src

SOURCE := osif_mn.c osif_tsk.c osif_tim.c osif_cfg.c xmtx.c

LintDirArg := $(addprefix +libm$(LPAREN)$(_CURDIR)\,$(addsuffix $(RPAREN),$(SOURCE)))
