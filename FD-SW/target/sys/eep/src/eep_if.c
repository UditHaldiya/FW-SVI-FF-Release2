/*
Copyright 2013-2014 by Dresser, Inc., as an unpublished trade secret.  All rights reserved.
This document and all information herein are the property of Dresser, Inc.
It and its contents are  confidential and have been provided as a part of
a confidential relationship.  As such, no part of it may be made public,
decompiled, reverse engineered or copied and it is subject to return upon
demand.

Based in part on the original Softing code whose copyright notice follows
*/

/*****************************************************************************
*                                                                            *
*                     SOFTING Industrial Automation GmbH                     *
*                          Richard-Reitzner-Allee 6                          *
*                                D-85540 Haar                                *
*                        Phone: ++49-89-4 56 56-0                            *
*                          Fax: ++49-89-4 56 56-3 99                         *
*                                                                            *
*                            SOFTING CONFIDENTIAL                            *
*                                                                            *
*                     Copyright (C) SOFTING IA GmbH 2012                     *
*                             All Rights Reserved                            *
*                                                                            *
* NOTICE: All information contained herein is, and remains the property of   *
*   SOFTING Industrial Automation GmbH and its suppliers, if any. The intel- *
*   lectual and technical concepts contained herein are proprietary to       *
*   SOFTING Industrial Automation GmbH and its suppliers and may be covered  *
*   by German and Foreign Patents, patents in process, and are protected by  *
*   trade secret or copyright law. Dissemination of this information or      *
*   reproduction of this material is strictly forbidden unless prior         *
*   written permission is obtained from SOFTING Industrial Automation GmbH.  *
******************************************************************************
******************************************************************************
*                                                                            *
* PROJECT_NAME             Softing FF/PA FD 2.42                             *
*                                                                            *
* VERSION                  FF - 2.42                                         *
*                          PA - 2.42 (beta)                                  *
*                                                                            *
* DATE                     16. April 2012                                    *
*                                                                            *
*****************************************************************************/

/* ===========================================================================

FILE_NAME          eep_if.c



FUNCTIONAL_MODULE_DESCRIPTION

=========================================================================== */
#include "keywords.h"
#define  MODULE_ID      (COMP_EEP + MOD_EEPIF)


INCLUDES
#include "base.h"
#include "hw_cfg.h"
#include "hw_if.h"
#include "eep_if.h"
#include <hw_i2c.h> //for i2c mutex
#include <eep_hw.h> //for T_EEP_ADDR
#include <fbif.h>

#include "osif.h"

#include "except.h"
#include "dbg_out.h"

#include "mnassert.h"
#include "fram.h"

GLOBAL_DEFINES


LOCAL_DEFINES

#define EEP_INIT_VALUE      'q'         /* EEP RAM image value after init   */

#define RETRY_LIMIT         3           /* Retry limit for unsucc. reads    */

#define EEP_NUM_COPIES 2

typedef struct _T_EEP_DSC {
    USIGN16             offset;            /* size of data block               */
    USIGN16             len;            /* size of data block               */
    //USIGN8 FAR_D *      start_addr;     /* start_addr of data block         */
    //T_CHKSUM FAR_D *    checksum;       /* pointer to block checksum        */
    u16 CheckWord;
} T_EEP_DSC;

//FUNCTION_DECLARATIONS

LOCAL VOID          eep_task_main (T_EVENT event);
static T_EEP_RESULT  eep_task_write(const T_EEP_DSC *p_blk_dsc);
static T_EEP_RESULT  eep_task_config(void);

LOCAL T_EEP_RESULT  eep_init_blocks (s8_least retry_limit);
LOCAL T_EEP_RESULT  eep_read_info (VOID);
LOCAL USIGN16       eep_build_dscr (VOID);
LOCAL USIGN16       eep_config_sort (VOID);
LOCAL T_EEP_RESULT  eep_param_check (USIGN8 block_id, USIGN16 offset, USIGN16 length);
//static void eep_put (u8_least block_id);
static void eep_set_cs (const T_EEP_DSC *block_dscr);

EXPORT_DATA

IMPORT_DATA

LOCAL_DATA

#define EEP_WRITE_BUFFERSZ (5*1024)

#pragma location="EEP_IMAGE"
static NO_INIT    u8 eep_ram_image[USABLE_EEPROM_SIZE];
#pragma location="EEP_IMAGE"
static NO_INIT    u16 write_buffer[EEP_WRITE_BUFFERSZ/sizeof(u16)];
#pragma location="EEP_IMAGE"
static NO_INIT    u16 testpad[EEP_NUM_COPIES][EEP_WRITE_BUFFERSZ/sizeof(u16)];

#include "da_eep.h"                               /* DATA SEGMENT DEFINITION */

STATIC NO_INIT  T_EEP_DSC         eep_dscr[EEP_NBR_OF_BLOCKS];
STATIC NO_INIT  T_EEP_INFO        eep_info;


#include "da_def.h"                                 /* DEFAULT DATA SEGMENT */

//---------------- eep code hardening effort -------------------------
/* eep data structures consist of
1. Configuration data (sizes etc.) modified before RTOS starts
    eep_info
    eep_dscr
2. State data
    eep_state
In NVMEM, eep_dscr turns into a (smaller) array of employed block descriptions;
it is protected by CRC kept in eep_info image. in turn, eep_info has its own CRC.
In RAM, runtime protection is provided by
    CRC of eep_info
    Checksum of each eep_dscr
    Checksum of eep_state
The test occurs just before eep write or test access
*/
#ifdef UNPACKED_ACCESS
#define EEP_BLK_DATA_OFFSET sizeof (USIGN32)
#else
#define EEP_BLK_DATA_OFFSET sizeof (T_CHKSUM)
#endif

/** \brief A helper to get base pointer of the store for an eep descriptor
\param dscr - a pointer to descriptor
\return a pointer to base store
*/
static void FAR_D *eep_GetChecksumAddr(const T_EEP_DSC *dscr)
{
    USIGN8 *p = eep_ram_image;
    p += dscr->offset + eep_info.start_data;
    return p;
}

/** \brief A helper to get pointer to data payload for an eep descriptor
\param dscr - a pointer to descriptor
\return a pointer to data payload
*/
static USIGN8 FAR_D *eep_GetDataAddr(const T_EEP_DSC *dscr)
{
    USIGN8 *p = eep_ram_image;
    p += dscr->offset + eep_info.start_data + EEP_BLK_DATA_OFFSET;
    return p;
}

#define USE_CHECKSUM 1

#if USE_CHECKSUM
#include "crc.h"
#endif

static void eep_TestControlObjects(void)
{
    u16 crc = calc_crc (&eep_info, MN_OFFSETOF(T_EEP_INFO, CheckWord));
    MN_RT_ASSERT(crc == eep_info.CheckWord);
    for(u8_least b = 0; b < EEP_NBR_OF_BLOCKS; b++)
    {
#if USE_CHECKSUM
        Struct_Test(T_EEP_DSC, &eep_dscr[b]);
#else
        crc = calc_crc((void *)&eep_dscr[b], MN_OFFSETOF(T_EEP_DSC, CheckWord));
        MN_RT_ASSERT(crc == eep_dscr[b].CheckWord);
#endif
    }
}

static void eep_init_descriptions(void)
{
    for(u8_least b = 0; b < EEP_NBR_OF_BLOCKS; b++)
    {
        eep_dscr[b].len        = 0;
        eep_dscr[b].offset   = 0;
#if USE_CHECKSUM
        STRUCT_CLOSE(T_EEP_DSC, &eep_dscr[b]);
#else
        eep_dscr[b].CheckWord = calc_crc((void *)&eep_dscr[b], MN_OFFSETOF(T_EEP_DSC, CheckWord));
#endif
    }
}

typedef struct eep_state_t
{
    u8 eep_blkwrite_req[EEP_NBR_OF_BLOCKS];
    BOOL eep_init_sequence;
    u16 CheckWord;
} eep_state_t;

static eep_state_t eep_state =
{
    .eep_blkwrite_req = {0}, //lint !e943 too few initializers: 0's OK and allowe by MISRA 3
    .eep_init_sequence = FALSE, //meaning: not first init
    .CheckWord = CRC_SEED,
};


//--------------- testing support API implementation -------------------------
/** \brief provides artifacts of eep_info
*/
void eep_GetInfo(T_EEP_INFO *dst)
{
    if(dst != NULL)
    {
        osif_disable_all_tasks();
        *dst = eep_info;
        osif_enable_all_tasks();
    }
}

/** \brief Provides a copy of eep block info
\param b - requested logical eep block number
\param offset - a pointer to offset destination
\param length - a pointer to length destination
*/
void eep_GetBlockInfo(u8 b, u32 *offset, u32 *length)
{
    osif_disable_all_tasks();
    if(b < EEP_NBR_OF_BLOCKS)
    {
        *offset = eep_dscr[b].offset + eep_info.start_data;
        *length = eep_dscr[b].len;
    }
    else
    {
        *offset = 0;
        *length = 0;
    }
    osif_enable_all_tasks();
}


/*######################  EEP Interface Functions  #########################*/

//-------------- pass-through helpers -----------------

/** \brief pass-through write helper
\param eeprom_addr - EEPROM address
\param src_addr - source address
\param count - number of bytes
\return completion code - 0 iff error
*/
ErrorCode_t eep_write_block(u16_least eeprom_addr, const void *src_addr, u16 count)
{
    ErrorCode_t err;
    i2c_AcquireMutex();
    err = fram_WriteExt(src_addr, eeprom_addr, count, 0);
    i2c_ReleaseMutex();
    return err;
}

/** \brief pass-through read helper
\param dst_addr - destination address
\param eeprom_addr - EEPROM address
\param count - number of bytes
\return completion code - 0 iff error
*/
ErrorCode_t eep_read_block(void *dst_addr, u32 eeprom_addr, u32 count)
{
    ErrorCode_t err;
    i2c_AcquireMutex();
    err = fram_ReadExt(dst_addr, eeprom_addr, count, 0);
    i2c_ReleaseMutex();
    return err;
}

/* EXPLANATION:
NvMutex aims at protecting NVMEM access from non-reentrant data structures in RAM
all the way down to physical layer.
This means that it must be acquired/released at a fairly high level.

Currently, we don't have access to the write sequences
<eep_provide_block,eep_start_write,eep_write>
made by Softing code provided without source.

So, we use a second (internal) mutex to protect against interactions between
Softing accesses and ours, in a hope that Softing does their part w.r.t.
data structures correctly.
This internal mutex doesn't affect the API at the call points.

When/If Softing call points can be protected, the internal mutex stuff
will become a redundant time-consuming thing and could/should be removed
*/
#define MUTEX_ENABLED 1
#define MUTEX_CONFLICT_WATCH 0

#if MUTEX_ENABLED != 0
static OS_RSEMA NvMutex;
static OS_RSEMA NvMutex_Internal;
#endif

void eep_CreateMutex(void)
{
#if MUTEX_ENABLED != 0
    OS_CREATERSEMA(&NvMutex);
    OS_CREATERSEMA(&NvMutex_Internal);
#endif
    i2c_CreateMutex();
}

#if MUTEX_ENABLED != 0
#if MUTEX_CONFLICT_WATCH != 0

extern u8 tcb2index(const OS_TASK *tcb);

typedef struct mutex_conflict_t
{
    u8 requestor;
    u8 owner;
} mutex_conflict_t;

mutex_conflict_t Mutex_conflict[10];
u8 Mutex_conflict_count;

mutex_conflict_t LocalMutex_conflict[10];
u8 LocalMutex_conflict_count;

#endif
#endif

void eep_AcquireMutex(void)
{
#if MUTEX_ENABLED != 0
    if((u8_least)OS_Request(&NvMutex) == 0) //return value 1: Resource was available, now in use by calling task; 0: Resource was not available.
    {
        //Mutex not available
#if MUTEX_CONFLICT_WATCH != 0
        u8 Mutex_conflict_count1 = Mutex_conflict_count;
        OS_TASK* tcb = OS_GetResourceOwner(&NvMutex);
        if(Mutex_conflict_count1 < NELEM(Mutex_conflict))
        {
            Mutex_conflict[Mutex_conflict_count1].owner = tcb2index(tcb);
            Mutex_conflict[Mutex_conflict_count1].requestor = osif_get_current_task_id();
            Mutex_conflict_count= Mutex_conflict_count1+1;
        }
#endif
        (void)OS_Use(&NvMutex); //wait; returned value larger than 1 means the resource was already locked by the calling task
    }
#endif
}

void eep_ReleaseMutex(void)
{
#if MUTEX_ENABLED != 0
    OS_Unuse(&NvMutex);
#endif
}

static void eep_AcquireLocalMutex(void)
{
#if MUTEX_ENABLED != 0
    if((u8_least)OS_Request(&NvMutex_Internal) == 0)
    {
        //Mutex not available
#if MUTEX_CONFLICT_WATCH != 0
        u8 Mutex_conflict_count1 = LocalMutex_conflict_count;
        OS_TASK* tcb = OS_GetResourceOwner(&NvMutex_Internal);
        if(Mutex_conflict_count1 < NELEM(LocalMutex_conflict))
        {
            LocalMutex_conflict[Mutex_conflict_count1].owner = tcb2index(tcb);
            LocalMutex_conflict[Mutex_conflict_count1].requestor = osif_get_current_task_id();
            LocalMutex_conflict_count = Mutex_conflict_count1+1;
        }
#endif
        (void)OS_Use(&NvMutex_Internal); //wait
    }
#endif
}

static void eep_ReleaseLocalMutex(void)
{
#if MUTEX_ENABLED != 0
    OS_Unuse(&NvMutex_Internal);
#endif
}

//--------- watchdog proxy ------------
static u32 Atomic_ wd_timestamp; //! last tickle of the watchdog

/** \brief eep task notification for watchdog
*/
static void eep_NotifyWatchdogProxy(void)
{
    wd_timestamp = (u32)OS_GetTime32();
}

#define EEP_WD_ALLOWANCE 2000U //! large wtachdog time allowance (in ms)
/** \brief Proxy for eep task watchdog checkin.
eep task can take long time (in excess of 1.1 s) because of
long blocks and multiple preemptions, so we give it
a time allowance. The proxy must be called reasonably
frequently from a therefore higher priority task
*/
void eep_WatchdogProxy(void)
{
    u32 diff;
    MN_ENTER_CRITICAL();
        u32 timestamp = (u32)OS_GetTime32();
        diff = timestamp - wd_timestamp;
    MN_EXIT_CRITICAL();
    if(diff < EEP_WD_ALLOWANCE)
    {
        osif_TickleWatchdog(EEP_TASK_ID);
    }
}
//---------- end wd proxy stuff ---------



FUNCTION GLOBAL T_EEP_RESULT eep_init(VOID)
/*----------------------------------------------------------------------------

FUNCTIONAL_DESCRIPTION

Initializes the EEP interface: sets EEP block description structure, then
initializes the physical EEPROM device interface by calling "eeprom_init" and
finally reads all data from the EEPROM device to the RAM image.

!! CAUTION !! This function is NOT reentrant. It may be executed only during
initialization before tasks are started or when task scheduling is disabled!

RETURN_VALUE
    EEP_OK              => completed successfuly
    EEP_READ_ERR        => EEP info block could not be read
    EEP_CS_ERR          => checksum invalid in min. one block
    EEP_INCONSISTENT    => block descriptions are inconsistent

----------------------------------------------------------------------------*/
{
LOCAL_VARIABLES
    T_EEP_RESULT            result;
    USIGN16                 retry_cnt;
    T_EEP_ADDR              data_size;

FUNCTION_BODY

    eep_init_descriptions();
    retry_cnt = 0;
    //eep_AcquireMutex();

        /* Read data structure info from EEPROM device */
        /*---------------------------------------------*/
    do
    {
        Reset_Watchdog();
        result = eep_read_info ();
        retry_cnt++;
    } while ((result != EEP_OK) && (retry_cnt < RETRY_LIMIT));

    if ( result != EEP_OK )
    {
        DBG_PRINT_WAIT_( ("eep_init: 'eep_read_info ()' failed (code %d)\n", result) );
    }
    else if ( (eep_info.size_eep_data + sizeof (T_EEP_INFO) + (eep_info.num_blocks * sizeof (T_EEP_BLOCK_DSC))) > USABLE_EEPROM_SIZE )
    {
        _EEP_WARNING(WRN_INCONS, 0, FKT_INIT);
        result = (EEP_NOT_AVAIL);
    }
    else
    {
        data_size = eep_build_dscr ();

        if ( data_size != eep_info.size_eep_data )
        {
            _EEP_WARNING(WRN_INCONS, 0, FKT_INIT);
            result = (EEP_NOT_AVAIL);
        }
        else
        {
            result = eep_init_blocks (RETRY_LIMIT - retry_cnt);
            if ( result != EEP_OK )
            {
                DBG_PRINT_WAIT_( ("eep_init: 'eep_init_blocks ()' failed (code %d)\n", result) );
            }
        }
    }

    if(result != EEP_OK)
    {
        eep_info.state = EEP_UNAVAIL;
    }

    //eep_ReleaseMutex();

    //Only now can we create the task to handle writes and tests
    osif_create_task (EEP_TASK_ID, EEP_TASK_PRIO, eep_task_main, OS_IF_EVENT_EEPROM);

    return (result);
} /* eep_init */



//Catalog handling
static USIGN16 eep_offset = 0;

static T_EEP_RESULT eep_compute_description(T_EEP_DSC *dscr, const T_EEP_BLOCK_DSC *p_eep_block_dsc)
{
    u8 i = p_eep_block_dsc->block_id;
    if ( i >= EEP_NBR_OF_BLOCKS )
    {
        return EEP_PARAM_ERR;
    }
    T_EEP_RESULT ret = EEP_OK;
    if( i != 0)
    {
        dscr[i].len        = p_eep_block_dsc->block_size;
        dscr[i].offset   = eep_offset;
        if(dscr[i].len > sizeof(write_buffer))
        {
            ret = EEP_PARAM_ERR;
        }
#if USE_CHECKSUM
        STRUCT_CLOSE(T_EEP_DSC, &eep_dscr[i]);
#else
        dscr[i].CheckWord = calc_crc((void *)&dscr[i], MN_OFFSETOF(T_EEP_DSC, CheckWord));
#endif

#ifdef UNPACKED_ACCESS
        /* align offset to multiple of 4 byte */
        eep_offset += ((dscr[i].len + 3u) & ~3u);
#else
        eep_offset += dscr[i].len;
#endif /* UNPACKED_ACCESS */
        eep_offset += EEP_BLK_DATA_OFFSET;
    }
    return ret;
}

//----------------- selftest business --------------------


typedef enum testphase_t
{
    eeptest_cat_r1, //read catalog first copy
    eeptest_cat_r2, //read catalog second copy
    eeptest_cat_w1, //write catalog first copy
    eeptest_cat_w2, //write catalog second copy

    eeptest_r1, //read block first copy
    eeptest_r2, //read block second copy
    eeptest_w1, //write block first copy
    eeptest_w2, //write block second copy
} testphase_t;
#define BLK_CATALOG ((u8)~0U)
/*NOTE: The eep test state variables are not protected because they are
self-repairing
*/
static u8 Atomic_ block_under_test = BLK_CATALOG; //start with the catalog
static u8 Atomic_ bad_block = 0;
static testphase_t testphase = eeptest_cat_r1;
static T_EEP_RESULT prev_result;

/** \brief Read a single copy of an eep block into destination buffer
Pre-requisite: eep_dscr must have been built
\param dst - a pointer to the destination buffer
\param b - block id (no range check!)
\param copy - a copy number to read (must be < EEP_NUM_COPIES but no range check)
\return an EEP error code
*/
static T_EEP_RESULT eep_readblock_helper(void *dst, u8_least b, u8_least copy)
{
    T_EEP_RESULT ret;
    /* read EEP data */
    osif_disable_all_tasks();
    T_EEP_ADDR eep_addr = eep_dscr[b].offset + eep_info.start_data;
    size_t header_size = EEP_BLK_DATA_OFFSET;
    u16 length = eep_dscr[b].len;
    osif_enable_all_tasks();
    size_t total_length = header_size + length;

    if ( eep_read_block(dst, eep_addr + copy*eep_info.size_eep_data,
                              total_length) != ERR_OK)
    {
        /* error in read command */
        ret= EEP_READ_ERR;
    }
    else
    {
        u16 cs = calc_crc ((u8 *)dst + header_size, length);

        if ( cs == *(u16*)dst )
        {
            ret = EEP_OK;
        }
        else
        {
            ret = EEP_CS_ERR;
        }
    }
    return ret;
}

/** \brief Write a single copy of an eep block from a source buffer
Pre-requisite: eep_dscr must have been built
\param src - a pointer to the source buffer
\param b - block id (no range check!)
\param copy - a copy number to write (must be < EEP_NUM_COPIES but no range check)
\return an EEP error code
*/
static T_EEP_RESULT eep_writeblock_helper(const void *src, const T_EEP_DSC *p_blk_dsc, u8_least copy)
{
    osif_disable_all_tasks();
    u16 total_length = EEP_BLK_DATA_OFFSET + p_blk_dsc->len;
    T_EEP_ADDR eep_addr = p_blk_dsc->offset + eep_info.start_data;
    osif_enable_all_tasks();
    T_EEP_RESULT ret = EEP_PRG_ERR;

    /* write EEP data */
    if (ERR_OK != eep_write_block (eep_addr + copy*eep_info.size_eep_data,
                              src, total_length) )
    {
        /* error in write command */
        _EEP_WARNING(WRN_EEPWRT, b, (USIGN32) eep_addr);
    }
    else
    {
        ret = EEP_OK;
    }
    return ret;
}

/** \brief Initialize eep test for the next block
*/
static void nextblock(void)
{
    testphase = eeptest_r1;
    u8 b = block_under_test + 1; //wraps around to 0 after BLK_CATALOG
    if(b == EEP_NBR_OF_BLOCKS)
    {
        b = BLK_CATALOG;
        testphase = eeptest_cat_r1;
    }
    else
    {
        testphase = eeptest_r1;
    }
    block_under_test = b;
}

/** \brief Phase 1 of eep block test: read the block under test into testpad[0]
*/
static void eeptest_eepblock_r1(void)
{
    u8 b = block_under_test;
    size_t len;
    len = eep_dscr[b].len; //OK to peek at an atomic member
    if (len == 0) //empty block
    {
        nextblock();
    }
    else
    {
        prev_result = eep_readblock_helper(testpad[0], b, 0);
        testphase = eeptest_r2;
    }
}

/** \brief Marks first bad block and advances the test to the next
*/
static void badblock(void)
{
    //both copies are bad
    if(bad_block == 0)
    {
        bad_block = block_under_test;
    }
    //no hope: - next block
    nextblock();
}


/** \brief Phase 2 of eep block test: read the block under test (copy 2) into testpad[1],
compare and decide whether repair is needed
*/
static void eeptest_eepblock_r2(void)
{
    u8 b = block_under_test;
    size_t len;
    len = eep_dscr[b].len; //OK to peek at an atomic member
    T_EEP_RESULT result = eep_readblock_helper(testpad[1], b, 1);
    if(prev_result == EEP_OK)
    {
        if(memcmp(testpad[0], testpad[1], len) == 0)
        {
            if(bad_block == b)
            {
                bad_block = 0;
            }
            //binary match - next block
            nextblock();
        }
        else
        {
            testphase = eeptest_w2;
        }
    }
    else
    {
        if(result == EEP_OK)
        {
            testphase = eeptest_w1;
        }
        else
        {
            badblock();
        }
    }
}

#define EEP_DIAG_REPORT_WORST_BLOCK 0
#define EEP_DIAG_ALLOW_DISABLE_WRITE 0

#if EEP_DIAG_REPORT_WORST_BLOCK
u32 Atomic_ eep_repair_count[EEP_NBR_OF_BLOCKS];
u8 Atomic_ worst_block_id;
#else
u16 Atomic_ eep_repair_count = 0;
#endif

u16 Atomic_ eep_repair_failed = 0;

/** \brief Phase 3 of eep block test: repair a failed copy from the opposite testpad[]
\param copy - a copy to write to (0 or 1)
*/
static void eeptest_eepblock_w(u8 copy)
{
    const T_EEP_DSC *p;
    p = &eep_dscr[block_under_test]; // Lint mistakenly believes that taking address of means access to.
    T_EEP_RESULT result = eep_writeblock_helper(testpad[1U-copy], p, copy); //The source is opposite
    if(result == EEP_OK)
    {
#if EEP_DIAG_REPORT_WORST_BLOCK
        eep_repair_count[block_under_test]++;
        if(eep_repair_count[block_under_test] > eep_repair_count[worst_block_id])
        {
            worst_block_id = block_under_test;
        }
#else
        eep_repair_count++;
#endif
    }
    else
    {
        eep_repair_failed++;
    }
    nextblock();
}

/** \brief A helper to read a copy of epp catalog into supplied buffers
\param[out] info - a pointer to catalog header
\param[out] block_info - pointer to catalog of eep blocks
\param copy - a copy number (0 or 1)
\return EEP error code
*/
static T_EEP_RESULT eep_read_catalog_helper(T_EEP_INFO *info, T_EEP_DSC *dsc, u8 copy)
{
    T_EEP_RESULT result = EEP_OK;
    //eep_AcquireLocalMutex();

    ErrorCode_t err = eep_read_block(info, 0+copy*sizeof(T_EEP_INFO), sizeof(T_EEP_INFO));
    if(err != ERR_OK)
    {
        result = EEP_READ_ERR;
    }
    else
    {
        //"OK" path
        u16 crc = calc_crc ((void *)info, MN_OFFSETOF(T_EEP_INFO, CheckWord));
        if(crc != info->CheckWord)
        {
            result = EEP_CS_ERR;
        }
    }

    T_EEP_BLOCK_DSC block_cfg;

    USIGN32 save = crc_start();

    for(size_t eebnum = 0; eebnum<info->num_blocks; eebnum++)
    {
        if(result != EEP_OK)
        {
            break;
        }
        else
        {
            err = eep_read_block(&block_cfg,
                                   sizeof(T_EEP_INFO)*EEP_NUM_COPIES +
                                       (copy*info->num_blocks + eebnum) * sizeof (T_EEP_BLOCK_DSC),
                                   sizeof (T_EEP_BLOCK_DSC));
            if(err != ERR_OK)
            {
                result = EEP_READ_ERR;
            }
            else
            {
                crc_feed(&block_cfg, sizeof(block_cfg));
                result = eep_compute_description(dsc, &block_cfg);
            }
        }
    }

    u16 crc = crc_end(save);

    if(result == EEP_OK)
    {
        if(crc != info->crc)
        {
            result = EEP_CS_ERR;
        }
    }
    //eep_ReleaseLocalMutex();
    return result;
}


/** \brief A helper to read a copy of epp catalog into a test buffer (no conversion)
\param copy - a copy number (0 or 1)
\return EEP error code
*/
static T_EEP_RESULT eeptest_catr(u8 copy)
{
    T_EEP_RESULT result = EEP_OK;
    eep_AcquireLocalMutex();

    T_EEP_INFO *info = (void *)testpad[copy];
    void *block_info = &info[1];
    ErrorCode_t err = eep_read_block((u8 *)info, 0+copy*sizeof(T_EEP_INFO), sizeof(T_EEP_INFO));
    if(err != ERR_OK)
    {
        result = EEP_READ_ERR;
    }
    else
    {
        //"OK" path
        u16 crc = calc_crc (info, MN_OFFSETOF(T_EEP_INFO, CheckWord));
        if(crc != info->CheckWord)
        {
            result = EEP_CS_ERR;
        }
    }

    if(result == EEP_OK)
    {
        err = eep_read_block(block_info,
                               sizeof(T_EEP_INFO)*EEP_NUM_COPIES + copy*info->num_blocks * sizeof (T_EEP_BLOCK_DSC),
                               info->num_blocks * sizeof (T_EEP_BLOCK_DSC));
        if(err != ERR_OK)
        {
            result = EEP_READ_ERR;
        }
    }

    if(result == EEP_OK)
    {
        u16 crc = calc_crc (block_info, info->num_blocks * sizeof (T_EEP_BLOCK_DSC));
        if(crc != info->crc)
        {
            result = EEP_CS_ERR;
        }
    }
    eep_ReleaseLocalMutex();
    return result;
}


/** \brief Phase 1 of eep catalog test: read the catalog into testpad[0]
*/
static void eeptest_catr1(void)
{
    prev_result = eeptest_catr(0);
    testphase = eeptest_cat_r2;
}

/** \brief Phase 2 of eep catalog test: read the catalog (copy 2) into testpad[1]
and decide whether a repair is needed
*/
static void eeptest_catr2(void)
{
    T_EEP_RESULT result = eeptest_catr(1);
    if(prev_result == EEP_OK)
    {
        T_EEP_INFO *info = (void *)testpad[0];
        if( (result == EEP_OK)
           && (memcmp(testpad[0], testpad[1], sizeof(T_EEP_INFO) + info->num_blocks * sizeof (T_EEP_BLOCK_DSC)) == 0) )
        {
            //match: OK
            nextblock();
        }
        else
        {
            testphase = eeptest_cat_w2;
        }
    }
    else
    {
        //will try to restore
        if(result == EEP_OK)
        {
            testphase = eeptest_cat_w1;
        }
        else
        {
            badblock();
        }
    }
}

/** \brief A helper to write a copy of epp catalog from supplied buffers
\param[in] info - a pointer to catalog header
\param[in] block_info - pointer to catalog of eep blocks
\param copy - a copy number (0 or 1) to write to
\return EEP error code
*/
static T_EEP_RESULT eep_writecat_helper(T_EEP_INFO *info, u8 copy)
{
    T_EEP_RESULT ret = EEP_OK;
    /* write EEP structure data */
    /*--------------------------*/
    ErrorCode_t err = ERR_OK;
    T_EEP_BLOCK_DSC block_cfg;
    //eep_AcquireLocalMutex();
    size_t eebnum = 0;
    u8_least b;

    USIGN32 save = crc_start();

    for(b=0; b<EEP_NBR_OF_BLOCKS; b++)
    {
        //Create T_EEP_BLOCK_DSC structures on the fly
        if(eep_dscr[b].len != 0)
        {
            block_cfg.block_id = (USIGN8)b;
            block_cfg.block_size = eep_dscr[b].len;
            block_cfg.auto_write_length = 0; //unused
            crc_feed(&block_cfg, sizeof(block_cfg));
            err = eep_write_block ((T_EEP_ADDR)(sizeof(T_EEP_INFO)*EEP_NUM_COPIES +
                                        (copy*info->num_blocks + eebnum) * sizeof (T_EEP_BLOCK_DSC)),
                                  &block_cfg,
                                  sizeof (T_EEP_BLOCK_DSC));
            if(err != ERR_OK)
            {
                break;
            }
            eebnum++;
        }
    }

    info->crc = crc_end(save);
    info->CheckWord = calc_crc((USIGN8 *)info, MN_OFFSETOF(T_EEP_INFO, CheckWord));


    if (ERR_OK != err)
    {
        ret = EEP_PRG_ERR;
    }

    if(ret == EEP_OK)
    {
        if (ERR_OK != eep_write_block (0 + copy*sizeof (T_EEP_INFO), (USIGN8 *)info, sizeof (T_EEP_INFO)) )
        {
            ret = EEP_PRG_ERR;
        }
    }
    //eep_ReleaseLocalMutex();
    return ret;
}

/** \brief Phase 3 of eep catalog test: repair a failed copy from the opposite testpad[]
\param copy - a copy to write to (0 or 1)
*/
static void eeptest_catw(u8 copy)
{
    T_EEP_INFO *info = (void *)testpad[1U-copy]; //source copy is the opposite

    T_EEP_RESULT result = eep_writecat_helper(info, copy);
    if(result == EEP_OK)
    {
#if EEP_DIAG_REPORT_WORST_BLOCK
        eep_repair_count[0]++;
        if(eep_repair_count[0] > eep_repair_count[worst_block_id])
        {
            worst_block_id = 0;
        }
#else
        eep_repair_count++;
#endif
    }
    else
    {
        eep_repair_failed++;
    }
    nextblock();
}

/** \brief A simple dispatcher of eep self-tests
*/
static void eeptest_eepblock(void)
{
    eep_TestControlObjects();
    switch(testphase)
    {
        case eeptest_cat_r1:
            eeptest_catr1();
            break;
        case eeptest_cat_r2:
            eeptest_catr2();
            break;
        case eeptest_cat_w1:
            eeptest_catw(0); //from 2 to 1
            break;
        case eeptest_cat_w2:
            eeptest_catw(1); //from 1 to 2
            break;
        case eeptest_r1:
            eeptest_eepblock_r1();
            break;
        case eeptest_r2:
            eeptest_eepblock_r2();
            break;
        case eeptest_w1:
            eeptest_eepblock_w(0); //from 2 to 1
            break;
        case eeptest_w2:
            eeptest_eepblock_w(1); //from 1 to 2
            break;
        default: //error: reinit
            testphase = eeptest_cat_r1;
            block_under_test = BLK_CATALOG;
            break;
    }
}

static u32 Atomic_ max_allwrite_time = 0;

/** \brief Provides access to internal stats
\param repair_count - a pointer to repair attempts
\param repair_failed - a pointer to repair failures
\param eep_write_time - a pointer max write time
\return first failed block
*/
u8 eeptest_GetStats(u32 *repair_count, u32 *repair_failed, u32 *eep_write_time)
{
    if(repair_failed != NULL)
    {
        *repair_failed = eep_repair_failed;
    }
    if(repair_count != NULL)
    {
#if EEP_DIAG_REPORT_WORST_BLOCK
        *repair_count = eep_repair_count[worst_block_id];
#else
        *repair_count = eep_repair_count;
#endif
    }
    if(eep_write_time != NULL)
    {
#if EEP_DIAG_REPORT_WORST_BLOCK
        *eep_write_time = (worst_block_id==0)?BLK_CATALOG:worst_block_id;
#else
        *eep_write_time = max_allwrite_time;
#endif
    }
    return bad_block;
}

#if EEP_DIAG_ALLOW_DISABLE_WRITE
static bool_t Atomic_ eep_write_disabled = false;
#endif

/** \brief A test API to inspect behavior (and time consumed) when all
eep blocks are written. Inspect max time by calling eeptest_GetStats().
\param clear_max_time - iff non-0, forget previous max time measument
\param full_load - iff non-0, force all eep blocks to be written.
*/
void eeptest_WriteAllBlocks(u8 clear_max_time, u8 full_load)
{
    if(clear_max_time != 0)
    {
        max_allwrite_time = 0;
    }
    switch(full_load)
    {
        case 1:
        {
            for(u8_least b=1; b<EEP_NBR_OF_BLOCKS; b++)
            {
                eep_put(b);
            }
            break;
        }
#if EEP_DIAG_ALLOW_DISABLE_WRITE
        case 2:
        {
            eep_write_disabled = true;
            break;
        }
#endif
        default:
        {
#if EEP_DIAG_ALLOW_DISABLE_WRITE
            eep_write_disabled = false;
#endif
            break;
        }
    }
}

//----------------- end selftest business --------------------

/****************************************************************************/

static T_EEP_RESULT eep_init_blocks (s8_least retry_limit)
/*----------------------------------------------------------------------------

FUNCTIONAL_DESCRIPTION

Checks the EEP blocks for consistency and initializes them to ROM defaults
if necessary

Pre-requisite: eep_dscr must have been built

RETURN_VALUE
    EEP_OK         => no error occured
    EEP_CS_ERR     => at least one block checksum inconsistent
    EEP_READ_ERR   => EEPROM could not be read

----------------------------------------------------------------------------*/
{
    T_EEP_RESULT  ret = EEP_OK; //OK if 0 blocks

    do
    {
        //eep_AcquireLocalMutex();
        memset(eep_ram_image, EEP_INIT_VALUE, sizeof(eep_ram_image));

        //Read one block at a time because of the watchdog (tickled automatically in the driver)
        for (u8_least b=0; b<EEP_NBR_OF_BLOCKS; b++)
        {
            if (eep_dscr[b].len != 0) //non-empty block
            {
                for(u8_least i=0; i<EEP_NUM_COPIES; i++)
                {
                    /* read EEP data */
                    Reset_Watchdog();
                    ret = eep_readblock_helper(eep_GetChecksumAddr(&eep_dscr[b]), b, i);
                    if(ret == EEP_OK)
                    {
                        break; //good read
                    }
                }
                if(ret != EEP_OK)
                {
                    eep_info.state = EEP_UNAVAIL; //mimic what Softing original did
                    break;
                }
            }
        }
        //eep_ReleaseLocalMutex();
        --retry_limit;
    } while ((ret != EEP_OK) && (retry_limit > 0));

    return (ret);
} /* eep_init_blocks */


/****************************************************************************/

FUNCTION GLOBAL T_EEP_RESULT eep_provide_block
  (
    IN  USIGN8      block_id,           /* ID of block                              */
    IN  USIGN16     block_len,          /* size of block                            */
    OUT VOID * *    p_ram_image         /* pointer to data image                    */
  )
/*----------------------------------------------------------------------------

FUNCTIONAL_DESCRIPTION

Checks existence / consistency of a block and provides a pointer to the block
start address, if successful, NULL in case of error.

RETURN_VALUE
    EEP_OK              => no error, '*p_ram_image' points to start of block
    EEP_NOT_EXIST       => block with ID 'block_id' does not exist
    EEP_INCONSISTENT    => size of block does not agree with 'block_len'
    EEP_CS_ERR          => checksum of block invalid -> data probably corrupt
    EEP_READ_ERR        => EEPROM could not be read

----------------------------------------------------------------------------*/
{
LOCAL_VARIABLES


FUNCTION_BODY
    T_EEP_RESULT ret;
    *p_ram_image = NULL;

    //Ensure eep states remain unchanged
    eep_AcquireLocalMutex();

    if ( eep_info.state == EEP_UNAVAIL )
    {
        DBG_OUT_NOWAIT_("eep_provide_block: EEPROM not available\n");
        ret = (EEP_READ_ERR);
    }
    else if ( eep_param_check (block_id, 0, 0) != EEP_OK )
    {
        DBG_PRINT_NOWAIT_( ("eep_provide_block: block %d not existent\n", block_id) );
        ret = (EEP_NOT_EXIST);
    }
    else if ( block_len != eep_dscr[block_id].len )
    {
        DBG_PRINT_NOWAIT_( ("eep_provide_block: block %d not consistent\n", block_id) );
        ret = (EEP_INCONSISTENT);
    }
    else
    {
        *p_ram_image = eep_GetDataAddr(&eep_dscr[block_id]);
        /* NOTE:
        Softing used to check the CRC of the block here.
        Since
        1. we compute the CRC in the buffer
        2. the original implementation is not thread-safe
        there is no value in maintaining the CRC live
        */
        ret = EEP_OK;
    }

    eep_ReleaseLocalMutex();
    return ret;
} /* FUNCTION eep_provide_block */


//lint -esym(818, p_eep_block_dsc) can't change Softing public prototype to make it const*
/******************************************************************************/
FUNCTION GLOBAL T_EEP_RESULT eep_config_change
  (
    IN USIGN8               no_of_eep_block_dsc,
    IN T_EEP_BLOCK_DSC *    p_eep_block_dsc
  )
/*------------------------------------------------------------------------------
FUNCTIONAL_DESCRIPTION
Inserts 'no_of_eep_block_dsc' block descriptions into the global descriptor
structure. If descriptions for certain block IDs are already existing, they
are overwritten.

possible return values:
    EEP_OK              => completed successfully
    EEP_PARAM_ERR       => block ID(s) exceed limit

------------------------------------------------------------------------------*/
{
LOCAL_VARIABLES
    T_EEP_RESULT ret = EEP_OK;

FUNCTION_BODY

    //eep_AcquireMutex();

    if (eep_info.state != EEP_IN_CONFIG)
    {
        eep_init_descriptions();
        eep_info.state = EEP_IN_CONFIG;
        eep_offset = 0; //start over
    }

    for (USIGN8 i=0; i<no_of_eep_block_dsc; i++)
    {
        if ( p_eep_block_dsc[i].block_id == 0 )
        {
            /* ID 0 is not valid, but indicates an unused entry */
        }
        else
        {
            ret = eep_compute_description(eep_dscr, &p_eep_block_dsc[i]);
            if(ret != EEP_OK)
            {
                break;
            }
        }
        eep_set_cs (&eep_dscr[p_eep_block_dsc[i].block_id]);


    } /* end for i */

    //eep_ReleaseMutex();

    return ret;
} /* FUNCTION eep_config_change */


/******************************************************************************/

FUNCTION GLOBAL T_EEP_RESULT eep_config_act (VOID)
/*------------------------------------------------------------------------------
FUNCTIONAL_DESCRIPTION
Activates the new EEP configuration, i.e. builds new management structures and
writes the structure layout information to the EEPROM (if successful)

possible return values:
    EEP_OK          => activation succeeded
    EEP_MEM_INSUFF  => data does not fit into EEPROM
    EEP_PRG_ERR     => error while programming EEPROM

------------------------------------------------------------------------------*/
{
LOCAL_VARIABLES
    USIGN16 data_size, block_num;
    u16_least total_size;
    T_EEP_RESULT ret;

FUNCTION_BODY

    //eep_AcquireMutex();

    block_num = eep_config_sort ();

        /* Init block descriptions */
        /*-------------------------*/
    data_size = eep_build_dscr ();
    total_size = data_size + sizeof (T_EEP_INFO) + (block_num * sizeof (T_EEP_BLOCK_DSC));
    if ( total_size > USABLE_EEPROM_SIZE )
    {
        DBG_PRINT_( ("eep_config_act: EEP_MEM_INSUFF (block_num = %d, data_size = %d)\n", block_num, data_size) );
        ret = (EEP_MEM_INSUFF);
    }
    else
    {
        eep_info.size_eep_data = data_size;
        eep_info.num_blocks    = (u8)block_num;
        eep_info.start_data    = (T_EEP_ADDR)(EEP_NUM_COPIES*(sizeof (T_EEP_INFO) + (block_num * sizeof (T_EEP_BLOCK_DSC))));

        MN_ENTER_CRITICAL();
            storeMemberInt(&eep_state, eep_init_sequence, TRUE);
        MN_EXIT_CRITICAL();

        ret = eep_task_config();
    }

    //eep_ReleaseMutex();
    return ret;

} /* FUNCTION eep_config_act */


/******************************************************************************/

FUNCTION LOCAL USIGN16 eep_config_sort (VOID)
/*------------------------------------------------------------------------------
FUNCTIONAL_DESCRIPTION
Sorts the array of block configuration data in a way that all empty
entries are moved to the back end.

possible return values:
Returns the number of non-empty block configuration entries.
------------------------------------------------------------------------------*/
{
    USIGN16 i;
    USIGN16 k = 0; //kount of non-empty blocks

    for (i=0; i<EEP_NBR_OF_BLOCKS; i++)
    {
        if(eep_dscr[i].len != 0)
        {
            k++;
        }
    }
    return k;
} /* FUNCTION eep_config_sort */


/****************************************************************************/


/****************************************************************************/

FUNCTION GLOBAL T_EEP_RESULT eep_start_write
(
    IN  USIGN8   block_id           /* ID of block                          */
)
/*----------------------------------------------------------------------------

FUNCTIONAL_DESCRIPTION
Prepares a block for write operations.


RETURN_VALUE
    EEP_OK          => no error occured
    EEP_PARAM_ERR   => wrong parameter
    EEP_IN_CHANGE   => EEP configuration not consistent temporarily

----------------------------------------------------------------------------*/
{
LOCAL_VARIABLES

    T_EEP_RESULT  result;

FUNCTION_BODY

    //Ensure eep states remain unchanged
    eep_AcquireLocalMutex();

    /* Check parameters */
    result = eep_param_check (block_id, 0, 0);

    eep_ReleaseLocalMutex();
    return (result);

} /* FUNCTION eep_start_write */


/****************************************************************************/

FUNCTION GLOBAL T_EEP_RESULT eep_write
(
    IN  USIGN8   block_id,          /* ID of block                          */
    IN  USIGN16  offset,            /* data offset inside the block         */
    IN  USIGN16  length,            /* length of data in bytes              */
    IN  USIGN8   mode               /* selected write mode                  */
)
/*----------------------------------------------------------------------------

FUNCTIONAL_DESCRIPTION

Writes data to the RAM image and, if needed, to the EEPROM. The only valid
value for the 'mode' parameter is :
  EEP_WAIT          the function returns after the final write cycle is
                    completed in the EEPROM device
If 'offset' == 0 and 'length' == 0 the whole block is written.

RETURN_VALUE
    EEP_OK          => no error occured
    EEP_PARAM_ERR   => wrong parameter
    EEP_PRG_ERR     => error while programming EEPROM
    EEP_IN_CHANGE   => EEP configuration not consistent temporarily

----------------------------------------------------------------------------*/
{
LOCAL_VARIABLES

    T_EEP_RESULT  result;

FUNCTION_BODY

    /* Don't write when the length is 0 and not the */
    /* whole block should be written. */
    if ( (offset != 0) && (length == 0) )
    {
        return (EEP_OK);
    }

    /* ONLY EEP_WAIT is supported! - But we don't care (anymore) */
#if 0
    if ( mode != EEP_WAIT )
    {
        _ASSERT(FALSE);
        return (EEP_PARAM_ERR);
    }
#else
    UNUSED_OK(mode);
#endif //0
    eep_AcquireLocalMutex();
    /* Check other parameters */
    if ( (result = eep_param_check (block_id, offset, length)) == EEP_OK )
    {
        /* Write data to EEPROM area */
        eep_put (block_id);
    }
    eep_ReleaseLocalMutex();

    return (result);

} /* FUNCTION eep_write */



/****************************************************************************/

#if 0 //AK: This function is not used and hasn't been modified to account for any changes
FUNCTION GLOBAL VOID eep_prepare_extra_nv_block
(
    IN  USIGN8       block_id,       /* ID of block          */
    IN  USIGN8     * p_ram_addr,     /* address in image     */
    IN  USIGN16      ram_len,        /* length of this block */
    OUT USIGN32    * p_eep_addr      /* eeprom address       */
)
/*----------------------------------------------------------------------------

FUNCTIONAL_DESCRIPTION

  The function prepares the extra NV data for writing into
  the eeprom.

RETURN_VALUE

  None

----------------------------------------------------------------------------*/
{
LOCAL_VARIABLES

    T_CHKSUM    cs;

FUNCTION_BODY

    _ASSERT((block_id == NV_EXTRA_EEP_BLK) || (block_id == (NV_EXTRA_EEP_BLK + 1)));

    *p_eep_addr = (T_EEP_ADDR) ((USIGN8*)eep_dscr[block_id].start_addr - eep_ram_image) + eep_info.start_data;

    _ASSERT(ram_len != 0);
    _ASSERT(p_ram_addr != NULL);

    /* Calculate the checksum */
    cs = calc_crc (p_ram_addr, ram_len);
    *((T_CHKSUM *)(p_ram_addr - 2)) = cs;

    return;

} /* FUNCTION eep_prepare_extra_nv_block */
#endif

#if 0 //AK: This function is not used and hasn't been modified to account for two copies
FUNCTION GLOBAL VOID eep_put_extra_nv_data
(
    IN  USIGN32      eep_addr,       /* eeprom address       */
    IN  USIGN8     * p_ram_addr,     /* address in image     */
    IN  USIGN16      eep_len         /* length of this block */
)
/*----------------------------------------------------------------------------

FUNCTIONAL_DESCRIPTION

  The function copies the extra NV data into the eeprom.

RETURN_VALUE

  None

----------------------------------------------------------------------------*/
{
LOCAL_VARIABLES

FUNCTION_BODY

    eeprom_write_block(eep_addr, p_ram_addr, eep_len, NULL);

    return;

} /* FUNCTION eep_put_extra_nv_data */
#endif

/*#############################  EEP Task  #################################*/
#define EEP_ADDR_PREWRITE (65535 - 44) //still want to reserve mn_assert area, whether we'll use it or not

#ifndef NDEBUG
//lint -esym(551, writes_pending) not accessed
static u8 writes_pending = 0; //for debug only, count pending writes
#endif

/** \brief Finds and writes eep blocks (if any)
*/
static void eep_write_wrapper(void)
{
    for(u8_least i=0; i<EEP_NBR_OF_BLOCKS; i++)
    {
        u8 write_req;

        osif_disable_all_tasks();
        Struct_Test(eep_state_t, &eep_state);
        write_req = eep_state.eep_blkwrite_req[i];
        storeMemberInt(&eep_state, eep_blkwrite_req[i], 0);
        const T_EEP_DSC *p = &eep_dscr[i];
        osif_enable_all_tasks();

        if(write_req != 0)
        {
            if(block_under_test == i)
            {
                //reset the test for this block to avoid inconsistency
                testphase = eeptest_r1;
            }

            eep_TestControlObjects();
            T_EEP_RESULT result;
#if EEP_DIAG_ALLOW_DISABLE_WRITE
            if(eep_write_disabled)
            {
                result = EEP_OK; //just skip a write
            }
            else
#endif
            {
                (void)eep_write_block(EEP_ADDR_PREWRITE, &i, sizeof(i)); //don't sweat it if fails
                result = eep_task_write(p);
            }

            osif_disable_all_tasks();
            if(result == EEP_OK)
            {
#ifndef NDEBUG
                writes_pending--;
#endif
            }
            else
            {
                storeMemberInt(&eep_state, eep_blkwrite_req[i], 1); //will try next time
            }
            osif_enable_all_tasks();
        }
        eep_NotifyWatchdogProxy();
    }
}

static u32 capture=0U;

u32 eep_get_stamp(void)
{
    return capture;
}

/** \brief A lowest-priority mopup service for pending eep block writes and
self-test. Does NOT return!
For backward compatibility, consumes T_EVENT messages the same way as the original.
*/
static void eep_task_main (T_EVENT event)
{
    _ASSERT((event != OS_IF_EVENT_EEPROM) || (event != OS_IF_EVENT_SYSSTART));
    UNUSED_OK(event); //in case _ASSERT is empty

    __enable_interrupt(); //least priority task gets the system running

    //First, deal with the possibly incomplete write before reset
    eep_TestControlObjects();
    ErrorCode_t err = eep_read_block((u8 *)&block_under_test, EEP_ADDR_PREWRITE, sizeof(block_under_test));
    if((err == ERR_OK) && (block_under_test < EEP_NBR_OF_BLOCKS))
    {
        testphase = eeptest_r1;
        do
        {
            eep_NotifyWatchdogProxy();
            eeptest_eepblock();
        } while((testphase != eeptest_r1) && (testphase != eeptest_cat_r1));
    }
    block_under_test = BLK_CATALOG;
    testphase = eeptest_cat_r1;

    for(;;)
    {
        capture = (u32)OS_GetTime32(); //embOS doc is murky on type int, but from the description one would think it is conceptually unsigned

        //Commit pending writes
        eep_write_wrapper();

        //Do one step of background test here
        eep_NotifyWatchdogProxy();
        eeptest_eepblock();

        eep_NotifyWatchdogProxy();
        OS_Delay(200); //sleep for 200 ms
        eep_NotifyWatchdogProxy();
        u32 time_spent = (u32)OS_GetTime32() - capture;
        max_allwrite_time = MAX(max_allwrite_time, time_spent);
    }

} /* FUNCTION eep_task_main */


/** \brief A helper to atomically write an eep block (all copies).
Requires that it is called from a mop-up (lowest useful priority) task.
NOTE: This function doesn't return until writes are completed.
\param p_blk_dsc - a pointer to description of the block to write
\return EEP error code (EEP_OK if at least one copy was written OK)
*/
static T_EEP_RESULT eep_task_write(const T_EEP_DSC *p_blk_dsc)
{
    osif_disable_all_tasks();
    size_t header = EEP_BLK_DATA_OFFSET;
    size_t total_length = header + p_blk_dsc->len;
    memcpy(write_buffer, eep_GetChecksumAddr(p_blk_dsc), total_length);
    osif_enable_all_tasks();
    u16 crc = calc_crc ((u8 *)write_buffer + header, p_blk_dsc->len);
    osif_disable_all_tasks();
    write_buffer[0] = crc;
    osif_enable_all_tasks();

    T_EEP_RESULT ret = EEP_PRG_ERR;
    for(u8_least i=0; i<EEP_NUM_COPIES; i++)
    {
        /* write EEP data */
        T_EEP_RESULT result = eep_writeblock_helper(write_buffer, p_blk_dsc, i);
        if ( result == EEP_OK )
        {
            ret = EEP_OK; //only one success required
        }
    }

    return ret;
} /* FUNCTION eep_task_write */


/** \brief A helper to write all copies of the eep catalog.
NOTE: This function doesn't return until writes are completed.
\return EEP error code (EEP_OK if at least one copy was written OK)
*/
static T_EEP_RESULT eep_task_config(void)
{
    T_EEP_RESULT ret = EEP_PRG_ERR;
    T_EEP_INFO info = eep_info;
    info.state = EEP_VALID;

    for(u8 i=0; i<EEP_NUM_COPIES; i++)
    {
        /* write EEP catalog */
        /*--------------------------*/
        if(eep_writecat_helper(&info, i) == EEP_OK)
        {
            ret = EEP_OK; //only one success required
        }
    }

    if(ret == EEP_OK)
    {
        eep_info = info;
    }

    return ret;
} /* FUNCTION eep_task_config */

/*########################  Internal functions  ############################*/

/** \brief EEP catalog initializer from EEP.

Initializes the EEP interface: sets EEP block description structure, then
initializes the physical EEPROM device interface by calling "eeprom_init" and
finally reads all data from the EEPROM device to the RAM image.

\return EEP error code (EEP_OK if at least one eep copy was read OK)
*/
static T_EEP_RESULT eep_read_info(void)
/*----------------------------------------------------------------------------

FUNCTIONAL_DESCRIPTION


RETURN_VALUE
    EEP_OK          => writing of data to the RAM image, previously read from the
                       EEPROM device, was successful. No Error.
    EEP_READ_ERR    => it was not possible to read data from the EEPROM.
    EEP_CS_ERR      => checksum error.

----------------------------------------------------------------------------*/
{
    T_EEP_RESULT result;

        /* Initialize low level routines */
        /*-------------------------------*/
    fram_Initialize();
    USIGN8 page_size = 0; //whatever that was to mean - serial FRAM returned 0.

        /* get EEPROM info */
        /*-----------------*/

    //eep_AcquireLocalMutex();
    for(u8 i=0; i<EEP_NUM_COPIES; i++)
    {
        result = eep_read_catalog_helper(&eep_info, eep_dscr, i);
        if(result == EEP_OK)
        {
            if ( page_size != eep_info.page_size )
            {
                result = EEP_INCONSISTENT;
            }
            if(eep_info.num_blocks > EEP_NBR_OF_BLOCKS)
            {
                result = EEP_INCONSISTENT;
            }
        }
        if(result == EEP_OK)
        {
            break;
        }
    }

    if ( result != EEP_OK )
    {
        eep_info.page_size = page_size;
        eep_info.state = EEP_UNAVAIL;
    }
    //eep_ReleaseLocalMutex();
    return (result);
} /* FUNCTION eep_read_info */


/****************************************************************************/

FUNCTION LOCAL USIGN16 eep_build_dscr (VOID)
/*------------------------------------------------------------------------------
FUNCTIONAL_DESCRIPTION

The function builds the block configuration in the EEPROM.

possible return values:
- returns the data size used for the block configuration in the EEPROM
------------------------------------------------------------------------------*/
{
    //Need to rebuild the catalog because order may have changed
    eep_offset = 0;
    for(u8_least b=0; b<EEP_NBR_OF_BLOCKS; b++)
    {
        if(eep_dscr[b].len != 0)
        {
            const T_EEP_BLOCK_DSC tmp =
            {
                .block_id = (u8)b,
                .auto_write_length = 0,
                .block_size = eep_dscr[b].len
            };
            (void)eep_compute_description(eep_dscr, &tmp);
        }
    }
    return (eep_offset);
} /* FUNCTION eep_build_dscr */


/****************************************************************************/

FUNCTION LOCAL T_EEP_RESULT eep_param_check
(
    IN  USIGN8   block_id,         /* ID of block                           */
    IN  USIGN16  offset,           /* data offset inside the block          */
    IN  USIGN16  length            /* length of data in bytes               */
)
/*----------------------------------------------------------------------------

FUNCTIONAL_DESCRIPTION

Checks the calling parameters against EEP block description.

RETURN_VALUE
    EEP_OK          => parameters ok
    EEP_PARAM_ERR   => parameters do not agree with block description
    EEP_IN_CHANGE   => EEP configuration not consistent temporarily

----------------------------------------------------------------------------*/
{
        /* check current state of EEP */
    if ( eep_info.state != EEP_VALID )
    {
        return (EEP_IN_CHANGE);
    }

        /* Is block_id within the valid range? */
    if ( block_id >= EEP_NBR_OF_BLOCKS )
    {
        _EEP_WARNING(WRN_MAXBLKID, block_id, EEP_NBR_OF_BLOCKS);
        return (EEP_PARAM_ERR);
    }

    /* Get the descriptor of the desired block */
    const T_EEP_DSC *p_blk_dsc = &eep_dscr[block_id];

    /* Check if block is supported */
    if ( p_blk_dsc->len == 0 )
    {
        /* block not supprted */
        //_EEP_WARNING(WRN_INVBLKID, block_id, 0);
        return (EEP_PARAM_ERR);
    }


    /* Are the data within the block boundaries? */
    if ( (offset + length) > p_blk_dsc->len )
    {
        _EEP_WARNING(WRN_BLKLEN, block_id, offset + length);
        return (EEP_PARAM_ERR);
    }

    return (EEP_OK);
} /* FUNCTION eep_param_check */


/****************************************************************************/

/** \brief creates a request for an eep block to be written.
\param block_id - A block to write
\return EEP_OK
*/
void eep_put
(
    IN  u8_least   block_id          /* ID of block                          */
)
/*----------------------------------------------------------------------------

FUNCTIONAL_DESCRIPTION

Writes data to the RAM image and, if needed, to the EEPROM.

RETURN_VALUE
    EEP_OK         => no error occured
    EEP_PRG_ERR    => error while programming EEPROM

----------------------------------------------------------------------------*/
{
    osif_disable_all_tasks();
    if(eep_state.eep_blkwrite_req[block_id] == 0)
    {
        storeMemberInt(&eep_state, eep_blkwrite_req[block_id], 1);
#ifndef NDEBUG
        writes_pending++;
#endif
    }
    osif_enable_all_tasks();
} /* FUNCTION eep_put */

//No param check!!!
static size_t eep_FindNVOffset(u8 fb_id)
{
    size_t offset = 0;
    for(u8 id=0; id<fb_id; id++)
    {
        offset += fbif_block_descr[id].block_n_par_len;
    }
    return offset;
}

static T_EEP_RESULT eep_save_fb_nv(u8 nv_blk_id, const void *src_nv, size_t offset, size_t len)
{
    void *ram_img;
    eep_AcquireLocalMutex();
    T_EEP_RESULT result = eep_provide_block(nv_blk_id, eep_dscr[nv_blk_id].len, &ram_img);
    if(result == EEP_OK)
    {
        mn_memcpy((u8 *)ram_img + offset, src_nv, len);
        eep_put(nv_blk_id);
    }
    eep_ReleaseLocalMutex();
    return result;
}


/** \brief Copy STATIC and NV areas of a function block from some RAM to FB and
requests a (delayed) write to FRAM.
\param fb_id - FB id (to include, here, RB and TB)
\param src_static - a pointer to RAM source for STATIC data; NULL skips the operation
\param offs_static - offset in FB STATIC image of the object to write
\param len_static - length in FB STATIC image of the object to write
\param src_nv- a pointer to RAM source for NV data; NULL skips the operation
\param offs_nv - offset in FB NV image of the object to write
\param len_nv - length in FB NV image of the object to write
\return eep-type error code
*/
T_EEP_RESULT eep_SaveFB(u8 fb_id,
                        const void *src_static, u16 offs_static, u16 len_static,
                        const void *src_nv, u16 offs_nv, u16 len_nv)
{
    if(fb_id >= fbif_dictionary.no_of_blocks)
    {
        return EEP_PARAM_ERR;
    }

    u8 eep_id = fb_id + EEP_FBL_BLK1;
    //STATIC param check
    if((src_static != NULL) && (eep_dscr[eep_id].len > (offs_static+len_static)))
    {
        return EEP_PARAM_ERR;
    }

    void *ram_img;
    T_EEP_RESULT result = EEP_OK;


    //NV data
    if((src_nv != NULL) && (fbif_block_descr[fb_id].block_n_par_len != 0))
    {
        size_t offset = eep_FindNVOffset(fb_id);
        if( (offset + fbif_block_descr[fb_id].block_n_par_len) > eep_dscr[EEP_NV_BLK1].len)
        {
            result = EEP_INCONSISTENT;
        }
        else if((offs_nv + len_nv) > eep_dscr[EEP_NV_BLK1].len)
        {
            result = EEP_PARAM_ERR;
        }
        else
        {
            offset += offs_nv;
            result = eep_save_fb_nv(EEP_NV_BLK1, src_nv, offset, len_nv);
            (void)eep_save_fb_nv(EEP_NV_BLK2, src_nv, offset, len_nv); //don't care; for compatibility with Softing
        }
    }

    //STATIC data
    if(result == EEP_OK)
    {
        if((src_static != NULL) && (fbif_block_descr[fb_id].block_eep_len != 0))
        {
            result = eep_provide_block(eep_id, eep_dscr[eep_id].len, &ram_img);
            if(result == EEP_OK)
            {
                eep_AcquireLocalMutex();
                mn_memcpy((u8 *)ram_img + offs_static, src_static, len_static);
                eep_put(eep_id);
                eep_ReleaseLocalMutex();
            }
        }
    }
    return result;
}

//From FB to some RAM
/** \brief Copy STATIC and NV images of a function block to some RAM.
\param fb_id - FB id (to include, here, RB and TB)
\param dst_static - a pointer to RAM destination for STATIC data; NULL skips the operation
\param offs_static - offset in FB STATIC image of the object to read
\param len_static - length in FB STATIC image of the object to read
\param dst_nv- a pointer to RAM destination for NV data; NULL skips the operation
\param offs_nv - offset in FB NV image of the object to read
\param len_nv - length in FB NV image of the object to read
\return eep-type error code
*/
T_EEP_RESULT eep_ReadFB(u8 fb_id, void *dst_static, u16 len_static, void *dst_nv, u16 len_nv)
{
    if(fb_id >= fbif_dictionary.no_of_blocks)
    {
        return EEP_PARAM_ERR;
    }

    u8 eep_id = fb_id + EEP_FBL_BLK1;
    void *ram_img;
    T_EEP_RESULT result = EEP_OK;

    eep_AcquireMutex();
    //NV data; don't care about the second copy
    if(fbif_block_descr[fb_id].block_n_par_len != 0)
    {
        size_t offset = 0;
        for(u8 id=0; id<fb_id; id++)
        {
            offset += fbif_block_descr[id].block_n_par_len;
        }
        if( (offset + fbif_block_descr[fb_id].block_n_par_len) > eep_dscr[EEP_NV_BLK1].len)
        {
            result = EEP_INCONSISTENT;
        }
        else
        {
            result = eep_provide_block(EEP_NV_BLK1, eep_dscr[EEP_NV_BLK1].len, &ram_img);
            if(result == EEP_OK)
            {
                mn_memcpy(dst_nv, (u8 *)ram_img + offset, len_nv);
                eep_put(EEP_NV_BLK1);
            }
        }

    }

    //STATIC data
    if(result == EEP_OK)
    {
        if(fbif_block_descr[fb_id].block_eep_len != 0)
        {
            result = eep_provide_block(eep_id, len_static, &ram_img);
            if(result == EEP_OK)
            {
                mn_memcpy(dst_static, ram_img, len_static);
            }
        }
    }
    eep_ReleaseMutex();
    return result;
}



/****************************************************************************/

static void eep_set_cs
(
    const T_EEP_DSC *block_dscr         /* descritor of block */
)
/*----------------------------------------------------------------------------
FUNCTIONAL_DESCRIPTION

This function calculates the chechsum of the block data including the version
number and store it at the end of the block

RETURN_VALUE
    none
----------------------------------------------------------------------------*/
{
LOCAL_VARIABLES

    T_CHKSUM         cs;

FUNCTION_BODY

    /* calculate checksum */
    cs = calc_crc (eep_GetDataAddr(block_dscr), block_dscr->len);

    T_CHKSUM *caddr = eep_GetChecksumAddr(block_dscr);
    *caddr = cs;

    return;
} /* FUNCTION eep_set_cs */


/****************************************************************************/

FUNCTION GLOBAL BOOL eep_startup_for_the_first_time (VOID)

/*----------------------------------------------------------------------------
FUNCTIONAL_DESCRIPTION

  The function checks if the device was started for the first time.

RETURN_VALUE

  TRUE    Device was started for the first time or was re-initialized.

  FALSE   Device was NOT started for the first time.

----------------------------------------------------------------------------*/
{
    Struct_Test(eep_state_t, &eep_state);
    return eep_state.eep_init_sequence;
} /* FUNCTION eep_startup_for_the_first_time */

/****************************************************************************/


