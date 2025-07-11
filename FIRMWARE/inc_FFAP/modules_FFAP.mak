ifeq (yin,yang)

This file contains a collection of modules the project uses.
It may be empty (for a project not using modules) but it must exist.

A module may be listed as foo_x-y.mak,z which references revision z
or as foo_x-y.mak which references the latest revision.
All traceable builds are required to specify explicit revisions
of all modules.

    Owner: AK
    modules_FFAP.mak
endif

#MUST go first
PREMODULES:=projconf_1-2.mak,47074

#HART
SUBMODULES+=hartslave_1-0.mak,50793

SUBMODULES+=hcframework_2-2.mak,48478

#Silly kludges because we don't want to change FRAM layout
MOD_OVERRIDE+=hart.h hartconf.c

#Local UI
SUBMODULES+=uicommonapi_2-0.mak,37113
SUBMODULES+=uiengine_2-0.mak,37113 uiif_1-0.mak,31913
#LCD driver
SUBMODULES+=lcd_1-0.mak,27138
#ASCII utility
SUBMODULES+=asciiutils_1-0.mak,27138

#makebasicprimer_1-0.mak,2

#Build environment
SUBMODULES+=lint_2-0.mak,37037 \
    buildutils_1-0.mak,46879 \
    ideutils_1-0.mak,48472 \
    makefiles_3-1.mak,48472


SUBMODULES+= MNEnvir_3-0.mak,37113 \
    process_3-0.mak,48687 \
    faultman_1-1.mak,36754 \
    mnassert_1-1.mak,51701 \
    bufutils_1-0.mak,27138 \
    nvram_3-3.mak,48065 \
    resource_1-0.mak,27138 \
    errcodes_1-0.mak,27138

#General helpers
SUBMODULES+=numutils_1-0.mak,27138 \
    bitutils_1-0.mak,27138

#BIOS
SUBMODULES+=wprotect_1-0.mak,27138 \
    pbypass_1-0.mak,27138

#HART Glue layer
SUBMODULES+= corefac_1-0.mak,28702 \
    hart_base_1-0.mak,27138 \
    faultsif_1-0.mak,27138 \
    smoothingif_0-0.mak,27138 \
    pbypassif_1-0.mak,27138 \
    hwswitchesif_1-0.mak,29467 \
    pressuresif_1-0.mak,50822

#Since DI is present
SUBMODULES+=diswif_1-0.mak,27138


#SUBMODULES+=hcframework_1-0.mak,35209
SUBMODULES+=faultsimport_1-0.mak,27138

#NVMEM
SUBMODULES+=nvmemdump_1-0.mak,48200

#RTOS
SUBMODULES+=uC_OS_II_2_74_1-0.mak,27138 \
    taskhelpers_1-1.mak,35141 \
	osport_1-0.mak,47058

#I/O Subsystem
SUBMODULES+=temprmncb_1-0.mak,27138 \
    tempr_1-0.mak,27138 \
    smoothing_1-0.mak,27138 \
    signaldef_1-0.mak,27138 \
    lcduiio_2-0.mak,30922

#Tempcomp engine
SUBMODULES+=tempcomp_2-0.mak,30987

#Floating point
SUBMODULES+=engunits_1-0.mak,28562

#Timer
SUBMODULES+=timer_1-0.mak,35141

#Always the latest profiler module
ifeq ($(MN_INSTRUM),profiler)
SUBMODULES+= instrum_profiling_2-0.mak,T
endif




