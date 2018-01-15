# root level

FORCED_DEPENDENCY_LOCAL :=

#The order of include search is important: Local directories must go first
ISUBDIR:= includes inc_$(PROJ)

#Supported HART versions
export hart_versions=5
hcimport=$(PROJDIR)/../FIRMWARE/$(HELPERS)/hcimportn.mak
FORCED_DEPENDENCY_LOCAL+=hcgenerate
hcgenerate:force;$(MAKE) -f $(hcimport) fname=inc_$(PROJ)/ffhartdef
force:;


#And the rest go here
ISUBDIR+= ..\FIRMWARE\includes ../FIRMWARE/mn_instrum/noinstrum \
    ..\FIRMWARE\inc_FFAP \
    target/sys/osif.emb/inc \
    target/inc target/inc/segm target/inc/ff target/cfg \
    _embOS_CortexM_IAR\Start\Inc


# ------------ project adaptation -------------------
# - Device identification
include $(settings)
# - Softing compatibility
include forsofting.mak
CFLAGS_PROJ+= $(CCFLAGS)
export CFLAGS_PROJ

#---- Firmware hardening effort: const-ize
linker_redirect=$(join $(patsubst %,%=mn,$(1)),$(1))

Redirected = fbs_call_fdev_funct
#Doesn't work: Redirected += smdl_call_fdev_funct
Redirected += fdc_wd_reset_wd_counter
LDFLAGS_PROJ+= $(addprefix --redirect , $(call linker_redirect, $(Redirected)))

#rombank?=0
#LDFLAGS_PROJ += --config_def rombank=$(rombank)
#This is common for all MNCB projects and replaces Softing's rombank switch
LDFLAGS_PROJ += $(foreach m,ROMSTART ROMEND RAMSTART RAMEND,--config_def $m=0x$(MEMMAP_$m))

$(info LDFLAGS_PROJ=$(LDFLAGS_PROJ))
export LDFLAGS_PROJ

#linker redirects for Lint
LintDirArg += $(addprefix -d, $(call linker_redirect, $(Redirected)))
#---- End Firmware hardening

#We don't care about system headers' compliance w.r.t. standard guards
comma=,
LintDirArg += -estring(451, "$(TOOLDIR)\*") /*System headers standard guards */
LintDirArg += -estring(451, *\cs_def.h) /*Softing segment management may not have standard guards */
LintDirArg += -estring(962, MODULE_ID) -estring(767, MODULE_ID) /* Softing module management for their exception handling */


# ------------ end project adaptation -------------------

SUBDIR:= base target diagnostics utilities framework sysio services inc_$(PROJ)

