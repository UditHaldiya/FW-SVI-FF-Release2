/*
Copyright 2015 by Dresser, Inc., as an unpublished trade secret.  All rights reserved.
This document and all information herein are the property of Dresser, Inc.
It and its contents are  confidential and have been provided as a part of
a confidential relationship.  As such, no part of it may be made public,
decompiled, reverse engineered or copied and it is subject to return upon
demand.
*/
/**
    \file vertranpub.h
    \brief Public interfaces for NVMEM layout translation

    CPU: Any

    OWNER: AK

    \ingroup nvram
*/
#ifndef VERTRANPUB_H_
#define VERTRANPUB_H_
#include "errcodes.h"
extern ErrorCode_t vertran( void * const *src, void **dst, u16_least srcVer, u16_least dstVer, void (*func)(void));

#define NVXLATE_VERSION 0
#define NVXLATE_HEADERSZ 24 //in diag_t entries; a dogmatic number to please FF params

#define NVXLATE_DIAGTYPE_READ 93
#define NVXLATE_DIAGTYPE_WRITE 96
#define NVXLATE_DIAGTYPE_XLATE 95

enum nvbuf_index_t
{
    nvindex_buftype = 0,
    nvindex_bufversion = 1,
    nvindex_headersize = 2, //in diag_t entries
    nvindex_sizein = 3, //size of raw image in diag_t entries
    nvindex_sizeout = 4, //total size of image including layout table in diag_t entries
    nvindex_versionin = 5, //layout version number
    nvindex_versionout = 6 , //layout version number of the inactive bank (for now, reserved)
    nvindex_result1 = 7, //completion code, must match nvindex_result2
    nvindex_result2 = 8, //copy of completion code
    nvindex_version_stored = 9, //what is found in the other bank (for now, reserved)
    nvindex_initoffset = 10, //offset of the NVMEM buffer
    //nvindex_numentries = 11, //
    nvindex_thisbank = 11, //image source/destination
    nvindex_deviceid1 = 12,
    nvindex_deviceid2 = 13,
    nvindex_deviceid3 = 14 //for future expansion
};
//typedef enum nvbuf_index_tnvbuf_index_t;

//#define NVRW_OPTIONS (0U | NVRWOPT_USE_LEGACY_BANK)
#define NVRW_OPTIONS (0U)
#define  NVRWOPT_USE_LEGACY_BANK 0x01U
enum
{
    NVBANK_ACTIVE = 0,
    NVBANK_INACTIVE = 1,
    NVBANK_0 = 2,
    NVBANK_1 = 3
#if (NVRW_OPTIONS & NVRWOPT_USE_LEGACY_BANK) != 0
    ,NVBANK_2 = 4
#endif
};

enum
{
    NVRW_COMPLETION_OK = 0,
    NVRW_NOT_SUPPORTED = 1,
    NVRW_PROHIBITED = 2,
    NVRW_RWERROR = 3,
    NVRW_DATA = 4,
    NVRW_SIGNATURE = 5,
    NVRW_WRONG_DEVICE = 6
};

#endif //VERTRANPUB_H_
/* This line marks the end of the source */
