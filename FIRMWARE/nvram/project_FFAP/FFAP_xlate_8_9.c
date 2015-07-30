/*
Copyright 2015 by Dresser, Inc., as an unpublished trade secret.  All rights reserved.
This document and all information herein are the property of Dresser, Inc.
It and its contents are  confidential and have been provided as a part of
a confidential relationship.  As such, no part of it may be made public,
decompiled, reverse engineered or copied and it is subject to return upon
demand.
*/
/**
    \file FFAP_xlate_8_9.c
    \brief Trivial translation between layouts 8 (bank 2) and 9 (same but
            in bank 0 or 1 corresponding to the build)

    CPU: Any

    OWNER: AK

    \ingroup nvram
*/
#include "mnwrap.h"
#include "FFAP_xlate_8_9.h"

/** \A trivial translation routine
NOTE: Never touch id 0!
*/
ErrorCode_t FFAP_xlateup_8_9(const u8 *src, u8 *dst, void (*func)(void))
{
    for(u8_least id = 1U; id<FFAP_NVRAM_ENTRIES_VER_8; id++)
    {
        mn_memcpy(dst + FFAP_nvram_map_9[id].offset, src + FFAP_nvram_map_8[id].offset, FFAP_nvram_map_8[id].length);
        if((id%8U)==0U) //we CAN copy 8 records without watchdog, cant' we?
        {
            func();
        }
    }
    return ERR_OK;
}

ErrorCode_t FFAP_xlatedn_9_8(u8 *dst, const u8 *src, void (*func)(void))
{
    return FFAP_xlateup_8_9(src, dst, func);
}

/* This line marks the end of the source */
