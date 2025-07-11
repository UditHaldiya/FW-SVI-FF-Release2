TF = "$(ProgramFiles)\Microsoft Visual Studio 10.0\Common7\IDE\tf.exe"
TFPT = "$(ProgramFiles)\Microsoft Team Foundation Server 2010 Power Tools\tfpt.exe"

appserverc = $(firstword $(shell cmd /C sort /R appserverc.txt))
synccmd = $(TF) get . /recursive /noprompt /overwrite
modcmd = gnumake proj=FFAP MODULES
out_dir := C:\FF_Auto_Builds\FromReleasesBranch\FromRelease2\ForHoneywell
buildname = C$(appserverc)
uniqroot = C:\tfsbuildR\SVIFF\Release2\ForHoneywell
OFFroot = $(uniqroot)\FIRMWARE
OFFmodroot = $(uniqroot)\Core\FIRMWARE

all : appserverc.txt
    @echo appserverc=$(appserverc)
    $(synccmd)
	$(modcmd)
    if not exist $(out_dir)\$(buildname) cmd /C mkdir $(out_dir)\$(buildname) && $(MAKE) OFFDir=Rel proj=FFAP plugin=ffplug.mak OFFICIAL notask=1 buildname=$(buildname) OFFver=$(buildname) \
	NO_MNS=1 OFFroot=$(OFFroot) OFFmodroot=$(OFFmodroot) MNS_OFFICIAL_DIR=$(out_dir)\$(buildname) avplugin=avplugin.mak silent=1 >$(out_dir)\$(buildname)\build.log 2>&1

appserverc.txt : force
    $(TF) history . /noprompt /sort:ascending /recursive | sed --text -e "$$!d" >$@
    $(TF) history ..\FD-SW /noprompt /sort:ascending /recursive | sed --text -e "$$!d" >>$@
	type appserverc.txt

force : ;

