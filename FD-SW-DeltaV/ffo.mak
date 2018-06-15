.PHONY: $(pretok)

FFTokenizerpath:=$(TokenizerBin)\tok\bin
dictfile:=$(TokenizerDir)\ddl\standard.dct
imagepath:=$(TokenizerDir)\ddl\htk
#Warining we need to suppress in tokenizer
SurpressWarning:=718,768
pretok_exe?=ffpretok.exe
tok_exe?=ff_tok32.exe
#pretok_exe:=ff5pretok.exe
#tok_exe:=ff5_tok32.exe

_tok: $(pretok)
    $(FFTokenizerpath)\$(tok_exe) -s $(TokenizerBin)\release $(pretok)
ifneq ($(filter -DDD4,$(option)),)
    $(MN_CP) $(basename).ffo $(dst)/
    $(MN_CP) $(basename).sym $(dst)/
else
    $(MN_CP) $(basename).ff5 $(dst)/
    $(MN_CP) $(basename).sy5 $(dst)/
endif

$(pretok) : $(DDLSRC)
    @echo option=$(option)
    $(FFTokenizerpath)\$(pretok_exe) $(option) -d $(dictfile) -w $(SurpressWarning) -I$(includepath) -R $(releasepath) -p "$(imagepath)" $< $@
	$(pause)

