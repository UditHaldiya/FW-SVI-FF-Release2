/*
Copyright 2015 by Dresser, Inc., as an unpublished trade secret.  All rights reserved.
This document and all information herein are the property of Dresser, Inc.
It and its contents are  confidential and have been provided as a part of
a confidential relationship.  As such, no part of it may be made public,
decompiled, reverse engineered or copied and it is subject to return upon
demand.
*/
/**
    \file nvram_xtable.c
    \brief Master NVMEM layout translation table

    CPU: Any

    OWNER: AK

    \ingroup nvram
*/
#include "mnwrap.h"
#include "evertran.h"
#include "FFAP_xlate_8_9.h"

const xlatef_t xTable[] =
{
    [0] = //not supported
    {
        .xup = NULL,
        .xdown = NULL,
        .num_entries = 0,
        .map = NULL,
    },
    [1] = //not supported
    {
        .xup = NULL,
        .xdown = NULL,
        .num_entries = 0,
        .map = NULL,
    },
    [2] = //not supported
    {
        .xup = NULL,
        .xdown = NULL,
        .num_entries = 0,
        .map = NULL,
    },
    [3] = //not supported
    {
        .xup = NULL,
        .xdown = NULL,
        .num_entries = 0,
        .map = NULL,
    },
    [4] = //not supported
    {
        .xup = NULL,
        .xdown = NULL,
        .num_entries = 0,
        .map = NULL,
    },
    [5] = //not supported
    {
        .xup = NULL,
        .xdown = NULL,
        .num_entries = 0,
        .map = NULL,
    },
    [6] = //not supported
    {
        .xup = NULL,
        .xdown = NULL,
        .num_entries = 0,
        .map = NULL,
    },
    [7] = //not supported
    {
        .xup = NULL,
        .xdown = NULL,
        .num_entries = 0,
        .map = NULL,
    },
    [8] =
    {
        .xup = FFAP_xlateup_8_9, //supported up but no "up" version yet
        .xdown = NULL,
        .num_entries = FFAP_NVRAM_ENTRIES_VER_8,
        .map = FFAP_nvram_map_8,
    },
    [9] =
    {
        .xup = NULL, //supported up but no "up" version yet
        .xdown = FFAP_xlatedn_9_8,
        .num_entries = FFAP_NVRAM_ENTRIES_VER_9,
        .map = FFAP_nvram_map_9,
    },
};

const nvramDumpMap_t nvmem_banks[NVMEM_NUMBANKS] =
{
    [0] = //to be used with flash bank 0
    {
        .offset = NVMEM_LEGACY_BANK_SIZE + NVMEM_BANK_GAP,
        .length = NVMEM_STD_BANK_SIZE,
    },
    [1] = //to be used with flash bank 1
    {
        .offset = (NVMEM_LEGACY_BANK_SIZE + NVMEM_BANK_GAP) + (NVMEM_STD_BANK_SIZE + NVMEM_BANK_GAP),
        .length = NVMEM_STD_BANK_SIZE,
    },
#if (NVRW_OPTIONS & NVRWOPT_USE_LEGACY_BANK) != 0
    [2] = //to be used with flash banks 0 and 1 for legacy builds (layout <=8)
    {
        .offset = 0,
        .length = NVMEM_LEGACY_BANK_SIZE,
    },
#endif
};

/* This line marks the end of the source */
