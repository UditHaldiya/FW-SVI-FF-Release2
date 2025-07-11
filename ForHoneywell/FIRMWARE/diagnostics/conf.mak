SOURCE+=vramptest.c datahog.c

ifeq ($(FEATURE_LOGFILES),USED)
SOURCE+=diagrw.c
endif
