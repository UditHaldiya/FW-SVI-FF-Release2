ifeq (yin,yang)
    $Author: Arkkhasin $
endif

modexec:=$(lastword $(MAKEFILE_LIST))
#mycopy:=xcopy /R/K/Y
mycopy:=.\cp.exe -f -p
OFFVCS?="$(ProgramFiles)\Microsoft Visual Studio 10.0\Common7\IDE\tf.exe"

# Version control interface to modules
VCS_MODULES_ROOT:=$$/Core/FIRMWARE
modules_path:=$(lastword $(shell $(OFFVCS) workfold $(VCS_MODULES_ROOT) $(LOGIN)))

export submodfile:=$(PROJDIR)/modules_$(PROJ)
export modreqfile:=$(PROJDIR)/reqmodules_$(PROJ)

.PHONY : modsetup all_of_module

# ----------- Outer shell part -----------------
MODULES : modsetup
    -mkdir modules
    $(MAKE) -f $(modexec) module=modules\modules_$(PROJ).mak all_of_module
    $(pause)
    $(MAKE) -f $(PROJDIR)$(SLASH)$(HELPERS)$(SLASH)modverify.mak
    $(pause)

# Set up environment and root module
modsetup : modprepare
	-mkdir modules
    $(mycopy) inc_$(PROJ)\modules_$(PROJ).mak modules\modules_$(PROJ).mak
	@echo modules_path=$(modules_path)
    $(pause)

modprepare : hcinit
    -cmd /C del /S modconf_$(PROJ).mak.candidate
    -$(MN_RM) $(submodfile)
    -$(MN_RM) $(modreqfile)
    -$(MN_RM) -f modules/*,*

hcinit : force
    $(foreach hartver, $(hart_versions), if exist $(hcmdroot)$(hartver).candidate $(MN_RM) $(hcmdroot)$(hartver).candidate)

modclean : modprepare
    -$(MN_RM) -r modconf_$(PROJ).mak

#debug helper
modverify :
    $(MAKE) -f $(PROJDIR)$(SLASH)$(HELPERS)$(SLASH)modverify.mak


# ^----------- End outer shell part -----------------

comma:=,
empty:=
space:= $(empty) $(empty)

ifneq ($(module),)
include $(firstword $(subst $(comma),$(space),$(module)))
export MOD_OVERRIDE
$(info module=[$(module)] REQUIRE=[$(REQUIRE)])
$(info SUB=$(SUB))
$(info SRC=$(SRC))
$(info MCPY=$(MCPY))
$(info ++++++++++++++++++++ included $(firstword $(subst $(comma),$(space),$(module))) )
#$(info $(shell pause))
endif

#Backward compatibility
HCMD-ver5=$(HCMD)
allhartcmd=$(foreach hartver, $(hart_versions), $(HCMD-ver$(hartver)))

FILEMEMBERS=$(SRC) $(HDR) $(ASRC) $(USRC) $(UHDR) $(allhartcmd)
MEMBERS=$(FILEMEMBERS) $(SUB) $(MCPY) $(MKINC)

LOGIN:=

mchangeset?=T
mitem=$(subst ;T,;$(mchangeset), $(subst $(comma),;,$@))
fitem=$(subst ;T,;$(mchangeset), $(subst $(comma),;,$^))

$(info allhartcmd=$(allhartcmd))

all_of_module: $(module) $(SUBMODULES) $(MEMBERS) $(allhartcmd) HCMD filelist.txt filecpy.butt modget
    $(MN_ECHO) $(addprefix modules\,$(SUBMODULES)) >> $(submodfile)

.PHONY :$(MEMBERS) filelist.txt filecpy.butt

#The next rule's dependency is:
#   from $(FILEMEMBERS), filter out files in any directory and of any version, if basename is listed in $(MOD_OVERRIDE).
#   The sequence can be clarified as follows:
# Remove directories: A=$(notdir $(FILEMEMBERS))
# Pattern to match any version: B=$(addsuffix %, $(MOD_OVERRIDE))
# Files to remove from any directory: C=$(filter $(B), $(A)) [mod_test_removal]
# Make a directory pattern for file removal D=$(addprefix %, $(C))
# Remove the files in any directory: E=$(filter-out $(D), $(FILEMEMBERS))
mod_test_removal = $(filter $(addsuffix %, $(MOD_OVERRIDE)), $(notdir $1))

filelist.txt : $(filter-out $(addprefix %, $(filter $(addsuffix %, $(MOD_OVERRIDE)), $(notdir $(FILEMEMBERS)))), $(FILEMEMBERS))
    @echo MOD_OVERRIDE=$(MOD_OVERRIDE)
    @echo effective files = $^
    $(pause)
	echo $(addprefix $(VCS_MODULES_ROOT)/,$(fitem)) \>>$@

filecpy.butt : $(FILEMEMBERS)
	
#Maybe, we don't want it. Or we can add a marker as before
$(module) : inc_$(PROJ)\modules_$(PROJ).mak


modget : $(FILEMEMBERS)
ifneq ($(strip $(FILEMEMBERS)),)
	echo. $(MODARG)  $(LOGIN)>>filelist.txt
	$(OFFVCS) @filelist.txt
    $(pawz)
	.\cp.exe filecpy.butt filecpy.bat
	filecpy.bat
endif


#Create read marker in the last operation
define modgetMod
    $(OFFVCS) get $(VCS_MODULES_ROOT)/modules_store/$(notdir $(mitem)) $(MODARG)  $(LOGIN)
	echo get $(SLASH)>filelist.txt
	$(MN_ECHO) Rem Files to copy > filecpy.butt
	$(mycopy) $(modules_path)\modules_store\$(firstword $(subst $(comma),$(space),$(notdir $@))) modules\$(firstword $(subst $(comma),$(space),$(notdir $@)))
    @echo got module $@
endef

modmember=$(subst /,\,$(firstword $(subst $(comma),$(space),$@)))

define modget
	$(if $(call mod_test_removal, $(modmember)),, echo $(mycopy) $(modules_path)\$(modmember) $(modmember) >> filecpy.butt)
    $(pause)
endef

modconf=$(dir $@)modconf_$(PROJ).mak.candidate
modcpy=$(subst ->,$(space),$@)
$(MCPY) : modconf=$(lastword $(modcpy)).candidate
$(MCPY) : SourceVar=$(firstword $(modcpy))

$(MKINC) : modconf=$(lastword $(modcpy)).candidate
$(MKINC) : SourceVar=include $(firstword $(modcpy))

$(SRC): SourceVar=SOURCE
$(ASRC): SourceVar=ASOURCE
$(USRC): SourceVar=UFILES_C
$(UHDR): SourceVar=UFILES_H
$(SUB): SourceVar=SUBDIR

$(info SourceVar=[$(SourceVar)])

define write_modconf
    $(MN_ECHO) $(SourceVar)+=$(firstword $(subst $(comma),$(space),$(notdir $@))) >> $(modconf)
endef

define write_var
    $(MN_ECHO) $(subst `escquote`,\\\",$(subst `semicolon`,;,$(subst `colon`,:,$(subst `space`, ,$(SourceVar))))) >> $(modconf)
endef

$(SUBMODULES) : filelist.txt filecpy.butt $(module) force
    $(MN_ECHO) $(REQUIRE) >> $(modreqfile)
    $(paws)
    $(modgetMod)
    $(Hide)echo Read marker >"$@"
	echo SUB=[$(SUB)]
    $(MN_ECHO) $(REQUIRE) >> $(modreqfile)
    $(MAKE) -f $(modexec) module=modules\$@ all_of_module

$(SRC) $(ASRC) $(USRC) $(UHDR) : $(SUBMODULES)
    @echo getting=$@ (module=$(module))
    $(pause)
    $(modget)
    $(write_modconf)

$(HDR) : $(SUBMODULES)
    $(modget)

#directory $(SUB) may exist hence force
$(SUB) : $(SUBMODULES) force
    $(write_modconf)
    if not exist $@ cmd.exe /E /C mkdir $(subst /,\,$@)
	$(pawz)

$(MCPY) $(MKINC) : $(SUBMODULES)
    echo SourceVar=[$(SourceVar)]
    echo modconf=[$(modconf)]
    $(write_var)
    $(pause)

#XML-defined HART 5,6 commands in the modules
comma:=,
HCMD : $(allhartcmd) $(SUBMODULES)
    $(Hide)($(MN_ECHO) Writing xml from the module) \
    $(foreach hartver, $(subst X,6,$(hart_versions)), && ($(MN_ECHO) $(filter %.xml, $(subst $(comma), ,$(HCMD-ver$(hartver)))) >> $(hcmdroot)$(hartver).candidate ))
    @echo Done
    $(pause)

$(allhartcmd) : $(SUBMODULES)
    $(Hide)$(modget)

# ---------------------------------------------------
force : ;
