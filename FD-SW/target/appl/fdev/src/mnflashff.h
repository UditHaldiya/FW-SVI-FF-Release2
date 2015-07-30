/*
Copyright 2015 by Dresser, Inc., as an unpublished work.  All rights reserved.

This document and all information herein are the property of Dresser, Inc.
It and its contents are  confidential and have been provided as a part of
a confidential relationship.  As such, no part of it may be made public,
decompiled, reverse engineered or copied and it is subject to return upon
demand.
*/
/**
    \file mnflashff.h
    \brief FFP-only flash download API

*/
#include <softing_base.h>
#include <ptb.h>
#include <eep_if.h>
#ifndef MNFLASHFF_H_
#define MNFLASHFF_H_
#include "mnhart2ff.h" //fferr_t
#include "mn_flash.h" //common with APP

typedef enum {NOTACTIVE, ACTIVATE0, ACTIVATE1} PARTN_STATE;
extern BOOL activate_fd_sw_internal  ( const PARTN_STATE st[2] );

extern u32 fwdk_WriteAppCPU  (const void *data, u32 addr, u8_least len, u8_least flags);
extern fferr_t fwdk_AdaptAppNvmem(void);

extern u32 fwdk_WriteAppCPU32(u8_least cmdtype, u32 value);
extern void *fwdk_GetVerInfo(void);
extern FFInfo_t *fwdl_GetVerInfo(void);
extern void fwdl_CreateVVerInfo(void);

#endif //MNFLASHFF_H_
/* This line marks the end of the source */

