/*
Copyright 2009 by Dresser, Inc., as an unpublished trade secret.  All rights reserved.
This document and all information herein are the property of Dresser, Inc.
It and its contents are  confidential and have been provided as a part of
a confidential relationship.  As such, no part of it may be made public,
decompiled, reverse engineered or copied and it is subject to return upon
demand.
*/
/**
    \file mn_flash_selftest.c
    \brief functions about flash selftest

    CPU: Any

    OWNER:
    $Date: 08/15/14 13:20p $
    $Revision: 1 $
    $Author: Eric Jiang $

*/
#include <limits.h>
#include <softing_base.h>
#include <hw_if.h>
#include "oswrap.h"
#include "pwm.h"
#include "faultpublic.h"
#include "selftest.h"
#include "mn_flash_selftest.h"

/* ========== Simplified faults handler for comaptibility with APP ==========
If it needs to grow, it may well move in a separate file
*/

typedef u16 ff_fault_t; //we may need to grow the size
//lint -esym(749, ff_faultbit_t::*) may or may not be referenced
typedef enum ff_faultbit_t
{
    FF_FAULT_NONE,
    FF_FAULT_FW_CHECKSUM,
    FF_FAULT_TMOUT_FLASH_TEST,
} ff_faultbit_t;


static const ff_faultbit_t ff_fault[] =
{
    [FAULT_FW_CHECKSUM] = FF_FAULT_FW_CHECKSUM,
    [FAULT_TMOUT_FLASH_TEST] = FF_FAULT_TMOUT_FLASH_TEST,
};

/** \brief Maps Standard APP fault code to a bitmask corresponding to FFP bit
\param ErrorCode - a Standard fault code
\return a single-bit-bitmap (or 0 if out of range)
*/
static ff_fault_t newfault(faultcode_fast_t ErrorCode)
{
    ff_fault_t ret = 0U;
    if(ErrorCode >= NELEM(ff_fault))
    {
        //nothing to do
    }
    else
    {
        ff_fault_t fault = 1U << ff_fault[ErrorCode]; //lint !e641 enums shall be allowed as shift RHS
        if(fault >= (CHAR_BIT*sizeof(ff_fault_t)))
        {
            //out of range: nothing to do
        }
        else
        {
            ret = fault;
        }
    }
    return ret;
}

typedef struct ff_faults_t
{
    ff_fault_t ff_fault;
    u16 CheckWord;
} ff_faults_t; //!< a simplistic fault storage type

static ff_faults_t ff_faults =
{
    .ff_fault = 0U,
    .CheckWord = CRC_SEED,
}; //! a simplistic fault storage

/** \brief A simplified variant of fault set routine
\param ErrorCode - a Standard fault code
*/
void error_SetFault(faultcode_fast_t ErrorCode)
{
    ff_fault_t fault = newfault(ErrorCode);
    if(fault != 0U)
    {
        MN_ENTER_CRITICAL();
            fault |= ff_faults.ff_fault;
            storeMemberInt(&ff_faults, ff_fault, fault);
        MN_EXIT_CRITICAL();
    }
}

/** \brief A simplified variant of test set routine
\param ErrorCode - a Standard fault code
\return true iff fault exists
*/
bool_t error_IsFault(faultcode_fast_t ErrorCode)
{
    bool_t ret = false;
    ff_fault_t fault = newfault(ErrorCode);
    if(fault != 0U)
    {
        MN_ENTER_CRITICAL();
            Struct_Test(ff_faults_t, &ff_faults);
            ret = (ff_faults.ff_fault & fault) != 0U;
        MN_EXIT_CRITICAL();
    }
    return ret;
}


/* NOTE: It is actually used by os_core.c (set a breakpoint in debugger to test,
or just grep).
uC/OS-II manages to confuse lint 8.00h with the way reference to the function is
in executable source in a header (which os_core.c is w.r.t. ucos_ii.c) which is
<>-included. Changing to ""-include exposes zillions of messages about os_core.c
*/

/** \brief OS idle task hook - used to compute free processor time.
    Also performs ongoing CRC check of flash
*/
void  OSTaskIdleHook (void)
{
    selftest_Init();

    for (;;)
    {
//        tick_t ticks = timer_GetTicks();
//        mn_ExpireAssert(); //Clobbers the fatal assert record if we survived to this point
        testload();

    }
}

/** \brief Action to take when a flash failure is detected
*/
void ffpflash_Fail(void)
{
    (void)hw_hm_set_baudrate(WRONG_HM_BAUDRATE); //reconfigure UART to fail IPC communications
    HART_set_active(DEACTIVATE, DEACTIVATE); //As a secondary defense, disable IPC using Softing API
}

/** \brief  the macro are overrides of APP handlers
*/
#define ffpflash_FailHook bios_WritePwm

/** \brief the macro-ized name is to override of APP handler of flash test fails
\param value (Unused)
*/
void ffpflash_FailHook(u16 value)
{
    UNUSED_OK(value);
    ffpflash_Fail();
}

/*This line marks the end of the file*/
