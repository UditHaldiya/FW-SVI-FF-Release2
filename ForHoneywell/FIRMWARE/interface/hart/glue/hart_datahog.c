/*
Copyright 2014 by Dresser, Inc., as an unpublished trade secret.  All rights reserved.
This document and all information herein are the property of Dresser, Inc.
It and its contents are  confidential and have been provided as a part of
a confidential relationship.  As such, no part of it may be made public,
decompiled, reverse engineered or copied and it is subject to return upon
demand.
*/
/**
    \file hart_datahog.c
    \brief HART glue layer for valve diagnostic signature read/write

    CPU: Any

    OWNER: AK

    \ingroup HARTapp
*/
#include "mnwrap.h"
#include "datahog.h"
#include "hartdef5.h"
#include "bufutils.h"
#include "hartfunc.h"

s8_least hartcmd_DataCollection(const u8 *src, u8 *dst)
{
    const Req_DataCollection_t *s = (const void *)src;
    UNUSED_OK(dst);
    ErrorCode_t err = ERR_OK;
    DatahogState_t hog;
    (void)datahog_Get(&hog);
    switch(util_GetU8(s->DataCollectionCommand[0]))
    {
        case HC_StartCollecting:
            hog.datamask = util_GetU8(s->DataCollectionBitmap[0]);
            hog.status = DatahogStart;
            hog.skip_count = util_GetU16(s->DataCollectionSkipCount[0]);
            hog.skipsleft = 0;
            break;
        case HC_StopCollecting:
            hog.status = DatahogStop;
            break;
        default:
            err = ERR_INVALID_PARAMETER;
            break;
    }

    if(err == ERR_OK)
    {
        err = datahog_Set(&hog);
    }
    UNUSED_OK(s->BufferId);
    return err2hart(err);
}

/* This line marks the end of the source */
