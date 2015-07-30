#Default compiler version
ver=6.50
#IDE: ver=5.50
export IDEBITSDIR=..\FIRMWARE\idebits6

#Lint configuration
LINTARG=codebase.lnt

MN_INSTRUM=noinstum

#Doxygen version and location
DOXVER?=1.8.5
DOXYGEN:="$(ProgramFiles)\doxygen_$(DOXVER)\bin\doxygen.exe"
#graphviz_dotpath:="$(ProgramFiles)\Graphviz2.32\bin"

# Local UI languages setup - handy to have here
# Space-separated list which must be a subset of column names in the spreadsheet
# ... and of names defined in $(HELPERS)\uitext.pl (.xls extractor helper script)
#LanguagePack ?= English French Spanish Portuguese Japanese Italian German

# ======== FFP-specific but made by FFAP build ===============
include ..\FIRMWARE\inc_FFAP\ffids.mak
