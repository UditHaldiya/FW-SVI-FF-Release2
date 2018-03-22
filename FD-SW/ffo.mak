.PHONY: $(pretok)

FFTokenizerpath:=$(TokenizerDir)\tok\bin
dictfile:=$(TokenizerDir)\ddl\standard.dct
imagepath:=$(TokenizerDir)\ddl\htk
#Warining we need to suppress in tokenizer
SurpressWarning:=718

_tok: $(pretok)
    $(FFTokenizerpath)\ff_tok32.exe -s $(TokenizerDir)\release $(pretok)
ifneq ($(filter -DDD4,$(option)),)
    $(MN_CP) $(basename).ffo $(dst)/
    $(MN_CP) $(basename).sym $(dst)/
else
    $(MN_CP) $(basename).ff5 $(dst)/
    $(MN_CP) $(basename).sy5 $(dst)/
endif

$(pretok) : $(DDLSRC)
    @echo option=$(option)
    $(FFTokenizerpath)\ffpretok.exe $(option) -d $(dictfile) -w $(SurpressWarning) -I$(includepath) -R $(releasepath) -p "$(imagepath)" $< $@
	$(pause)

