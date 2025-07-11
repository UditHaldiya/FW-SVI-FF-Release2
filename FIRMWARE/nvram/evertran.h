/*
Copyright 2015 by Dresser, Inc., as an unpublished trade secret.  All rights reserved.
This document and all information herein are the property of Dresser, Inc.
It and its contents are  confidential and have been provided as a part of
a confidential relationship.  As such, no part of it may be made public,
decompiled, reverse engineered or copied and it is subject to return upon
demand.
*/
/**
    \file evertran.h
    \brief API for embedded vertran (NVMEM layout translation)

     CPU: Any

    OWNER: AK

    \ingroup nvram
*/
#ifndef EVERTRAN_H_
#define EVERTRAN_H_
#include "errcodes.h"
#include "verdump_types.h"
#include "vertranpub.h"
#include "dimensions.h"
#include "mncbdefs.h"

typedef ErrorCode_t xlateUp_t(const u8 *src, u8 *dst, void (*func)(void));
typedef ErrorCode_t xlateDown_t(u8 *dst, const u8 *src, void (*func)(void));
//lint -esym(768, xlatef_t::*) OK if not referenced
struct xlatef_t
{
    xlateUp_t *xup;
    xlateDown_t *xdown;
    size_t num_entries;
    const nvramDumpMap_t *map;
};
typedef struct xlatef_t xlatef_t;

extern const xlatef_t xTable[MNCB_VERSION_NUMBER+1U];

#if (NVRW_OPTIONS & NVRWOPT_USE_LEGACY_BANK) != 0
#   define NVMEM_STD_BANK_SIZE 5000
#   define NVMEM_LEGACY_BANK_SIZE 2300 //2280
#   define NVMEM_BANK_GAP 100
#   define NVMEM_NUMBANKS 3
#else
#   define NVMEM_STD_BANK_SIZE NVRAM_PARTITION_REMAP_SIZE
#   define NVMEM_LEGACY_BANK_SIZE 0
#   define NVMEM_BANK_GAP 0
#   define NVMEM_NUMBANKS 2
#endif

extern const nvramDumpMap_t nvmem_banks[NVMEM_NUMBANKS];


#endif //EVERTRAN_H_
/* This line marks the end of the source */
