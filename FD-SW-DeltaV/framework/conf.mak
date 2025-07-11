#framework\conf.mak

SOURCE+= ../../FIRMWARE/framework/cortextrapc.c \
    ../../FIRMWARE/framework/mncbdefs.c \
    ../../FIRMWARE/framework/verinfo.c \
    ../../FIRMWARE/framework/bios/stm_flash.c \

SOURCE+=../../FIRMWARE/framework/bios/stm_timer.c stm_watchdogff.c

ASOURCE := ../../FIRMWARE/framework/cortextrap.s79 \
    ../../FIRMWARE/framework/endofimage.s79

ISUBDIR+=../../FIRMWARE/framework

