#Version control common configuration


#Version control root of the project
export VCS_PROJ_ROOT:="\mncb\dev\LCX2AP\firmware"

#Place for sharing xml commands definitions with tools
export VCS_SHARED_ROOT:="\Common Source\Dev\Current"


#Version control builder username
#Version control builder password

#Default compiler version
ver?=5.50


#Lint configuration
LINTARG=codebase.lnt

#Doxygen version and location
DOXVER?=1.8.5
DOXYGEN:="$(ProgramFiles)\doxygen_$(DOXVER)\bin\doxygen.exe"
#graphviz_dotpath:="$(ProgramFiles)\Graphviz2.32\bin"
#Supported HART versions
export hart_versions=5 X 7
#   Where the definitions are imported from
#Where the faults and UI texts are defined (currently, same file)
export TEXTDEF:=$(PROJDIR)\$(PROJ)mncb_FAULT_matrix.xls
export FAULTDEF:=$(PROJDIR)\$(PROJ)mncb_FAULT_matrix.xls
LanguagePack ?= 
