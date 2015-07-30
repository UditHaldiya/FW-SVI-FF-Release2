/*
Copyright 2009 by Dresser, Inc., as an unpublished trade secret.  All rights reserved.
This document and all information herein are the property of Dresser, Inc.
It and its contents are  confidential and have been provided as a part of
a confidential relationship.  As such, no part of it may be made public,
decompiled, reverse engineered or copied and it is subject to return upon
demand.
*/
/**
    \file mn_advanced.c
    \brief functions about parameter ADVANCED

    CPU: Any

    OWNER:
    $Date: 07/08/13 15:40p $
    $Revision: 1 $
    $Author: victoria huang $

*/
#include <softing_base.h>
#include <ptb.h>
#include <appl_int.h>
#include <fbif_dcl.h>

//Glue headers last
#include "mn_advanced.h"
#include "mnhart2ff.h"
#include "mn_fstate.h"
#include "bitutils.h"
#include "fbif_dcl.h"
#include "mnassert.h"


#define PRAMETER_STANDARD_DEFAULT    {0xffffffffu, 0xffffffffu, 0xffffffffu, \
                                     0xffffffu, 0, 0x7fffffffu, 0x180bfe00u, \
                                     0xfffffff8u, 0xffffffu, 0, 0x1fff, \
                                     0, 0, 0, 0, 0, 0, 0, 0, 0}

#define PARA_BYTE_IN_DWORD           4u
#define PARA_BYTE_MOD_IN_DWORD       3u
#define PARA_BIT_IN_BYTE             8u
#define PARA_BIT_IN_U16              16u
#define PARA_ADV_READ_DWORD          5u
#define PARA_ADV_READ_BYTES          20u
#define PARA_ADV_RWF_DWORD           12u
#define PARA_ADV_DWORD               13u
#define DEVICE_ID_BYTES              32u
#define PARA_ADV_REDWRT_BYTES        48u
#define PARA_ADV_REWRCHE_BYTES       52u
#define CAL_TOTAL_DEVICE_ID_DWORD    20u
#define CAL_TOTAL_DEVICE_ID_BYTES    80u

#define state_CommitAdvanced         0
#define state_WriteOne               1
#define state_CommitBuffer           2
#define state_DoNothing              3


typedef struct
{
    u16 ptb_index;
    u16 alert_offset;      //offset from the head of p_block
} UNIT_alert_info_t;

static const UNIT_alert_info_t ptb_param_readonly[] = {
    {
        .ptb_index = REL_IDX_PTB_TRAVEL_ACCUMULATION_A_ALERT,
        .alert_offset = MN_OFFSETOF(T_FBIF_PTB, travel_accumulation_a_alert)+MN_OFFSETOF(_TRAVEL_ALERT, enable)
    },
    {
        .ptb_index = REL_IDX_PTB_TRAVEL_ACCUMULATION_B_ALERT,
        .alert_offset = MN_OFFSETOF(T_FBIF_PTB, travel_accumulation_b_alert)+MN_OFFSETOF(_TRAVEL_ALERT, enable)
    },
    {
        .ptb_index = REL_IDX_PTB_CYCLE_COUNTER_A_ALERT,
        .alert_offset = MN_OFFSETOF(T_FBIF_PTB, cycle_counter_a_alert)+MN_OFFSETOF(_CYCLE_COUNTER_ALERT, enable)
    },
    {
        .ptb_index = REL_IDX_PTB_CYCLE_COUNTER_B_ALERT,
        .alert_offset = MN_OFFSETOF(T_FBIF_PTB, cycle_counter_b_alert)+MN_OFFSETOF(_CYCLE_COUNTER_ALERT, enable)
    },
    {
        .ptb_index = REL_IDX_PTB_NEAR_CLOSED_ALERT,
        .alert_offset = MN_OFFSETOF(T_FBIF_PTB, near_closed_alert)+MN_OFFSETOF(_NEAR_CLOSED_ALERT, enable)
    },
    {
        .ptb_index = REL_IDX_PTB_SUPPLY_PRESSURE_HI_ALERT,
        .alert_offset = MN_OFFSETOF(T_FBIF_PTB, supply_pressure_hi_alert)+MN_OFFSETOF(_SYSTEM_ALERT, component_5)
    },
    {
        .ptb_index = REL_IDX_PTB_SUPPLY_PRESSURE_LO_ALERT,
        .alert_offset = MN_OFFSETOF(T_FBIF_PTB, supply_pressure_lo_alert)+MN_OFFSETOF(_SYSTEM_ALERT, component_5)
    },
    {
        .ptb_index = REL_IDX_PTB_SUPPLY_PRESSURE_LOLO_ALERT,
        .alert_offset = MN_OFFSETOF(T_FBIF_PTB, supply_pressure_lolo_alert)+MN_OFFSETOF(_SYSTEM_ALERT, component_5)
    },
    {
        .ptb_index = REL_IDX_PTB_TEMPERATURE_HI_ALERT,
        .alert_offset = MN_OFFSETOF(T_FBIF_PTB, temperature_hi_alert)+MN_OFFSETOF(_SYSTEM_ALERT, component_5)},
    {
        .ptb_index = REL_IDX_PTB_TEMPERATURE_LO_ALERT,
        .alert_offset = MN_OFFSETOF(T_FBIF_PTB, temperature_lo_alert)+MN_OFFSETOF(_SYSTEM_ALERT, component_5)
    },
    {
        .ptb_index = REL_IDX_PTB_IP_DRIVE_CURRENT_HI_ALERT,
        .alert_offset = MN_OFFSETOF(T_FBIF_PTB, ip_drive_current_hi_alert)+MN_OFFSETOF(_IP_DRIVE_CURRENT_ALERT, enable)
    },
    {
        .ptb_index = REL_IDX_PTB_IP_DRIVE_CURRENT_LO_ALERT,
        .alert_offset = MN_OFFSETOF(T_FBIF_PTB, ip_drive_current_lo_alert)+MN_OFFSETOF(_IP_DRIVE_CURRENT_ALERT, enable)
    },
};

static const  u16 pFK_1 = 0x5AA5u;
static const  u16 pFK_2 = 0xA55Au;
static u32 sAdvancedBuffer[PARA_ADV_DWORD];
static u16 uAdvBufferFlag = 0;
/* \brief write advanced parameter
    \param in:
        bit: the bit of the uAdvBufferFlag want to set
        uAdvBufferFlag: the flag which is wanted to be set

    \return:
        no
*/
MN_INLINE void mn_SetBufferFlag(u8 bit)
{
    MN_ASSERT(bit < PARA_BIT_IN_U16);
    uAdvBufferFlag = (uAdvBufferFlag | (u16)(0x1u << bit));
}
/* \brief write advanced parameter
    \param in:
        bit: the bit of the uAdvBufferFlag you want to get

    \return:
        the bit is true or false
*/
MN_INLINE bool_t mn_GetBufferFlag(u8 bit)
{
    MN_ASSERT(bit < PARA_BIT_IN_U16);
    return ((uAdvBufferFlag & ((u16)(0x1u << bit))) != 0);
}

/** \brief Tests a bit in an array of characters
   \param in:
        byteIndex - the number of the byte need to get
        DeviceId - pointer to char array containing bitNumber
   \return:
        byte value;
*/
static u8 mn_GetByte(u8 byteIndex, const u32 *Array)
{
    u8 uRvalue, j, k, uLShift, uRShift;
    u32 temp;
    MN_ASSERT(Array != NULL);
    j = byteIndex / PARA_BYTE_IN_DWORD;
    k = byteIndex % PARA_BYTE_IN_DWORD;
    uLShift = PARA_BYTE_MOD_IN_DWORD - k;
    uLShift *= PARA_BIT_IN_BYTE;
    uRShift = PARA_BYTE_MOD_IN_DWORD * PARA_BIT_IN_BYTE;
    temp = Array[j] << uLShift;
    uRvalue =(u8) ( temp >> uRShift );
    return uRvalue;
}

/** \brief Tests a bit in an array of characters
    \param in:
         bitNumber - the number of the bit to set
         bitArray - pointer to u32 array containing bitNumber
         length - the size of the array (for range) in byte
   \return:
         true if bit set, false if not
*/
bool_t mn_GetBit(u8 bitNumber, const u32* Array, u8 length)
{
    u8 uByte, uByteIndex;

    MN_ASSERT(Array != NULL);
    uByteIndex = BYTENUM(bitNumber);
    MN_ASSERT(uByteIndex < length);
    uByte = mn_GetByte(uByteIndex, Array);
    return ((uByte & BYTEBITMASK(bitNumber))!=0);
}

/* \brief calculate the first 32 bytes of the arrays which is used to calculate
    the userkey.
   \param in:
        no
   \return:
        no
*/
static u16 calcFrst(const u32 *DeviceId, u16 FK)
{
    u16 Frst = FK;
    u8  uTempData;
    for (u8 i = 0; i < PARA_ADV_REDWRT_BYTES; i++)
    {
        uTempData = mn_GetByte(i, DeviceId);
        u16 xDvcId = uTempData ^ 0xAAU;
        u16 DvcId = uTempData & 0xFFU;
        if (i % 2 != 0)
        {
            Frst += DvcId;
            DvcId = (u16)(DvcId << 7);
            Frst += DvcId;
            DvcId = (u16)(DvcId << 4);
            Frst += DvcId;
        }
        else
        {
            Frst += xDvcId;
            xDvcId = (u16)(xDvcId << 7);
            Frst += xDvcId;
            xDvcId = (u16)(xDvcId << 4);
            Frst += xDvcId;
        }

    }
    return Frst;
}

/* \brief calculate the last 32 bytes of the arrays which is used to calculate
    the userkey.
    \param in:
        no
    \return:
        no
*/
static u16 calcScnd(const u32 *DeviceId, u16 FK)
{
    u16 Scnd = FK;
    u8  uTempData;
    for (u8 i = PARA_ADV_REDWRT_BYTES; i < CAL_TOTAL_DEVICE_ID_BYTES; i++)
    {
        uTempData = mn_GetByte(i, DeviceId);
        u16 xDvcId = uTempData ^ 0xAAU;
        u16 DvcId=uTempData & 0xFFU;
        if (i % 2 != 0)
        {
            Scnd += DvcId;
            xDvcId = (u16)(xDvcId << 7);
            Scnd += xDvcId;
            DvcId = (u16)(DvcId << 11);
            Scnd += DvcId;
        } else
        {
            Scnd += xDvcId;
            DvcId = (u16)(DvcId << 7);
            Scnd += DvcId;
            xDvcId = (u16)(xDvcId << 11);
            Scnd += xDvcId;
        }
    }
    return Scnd;
}

/* \brief compare the input FFKey is euqal to the UserKey or not.
    \param in:
        pDeviceId: advanced parameter read/write table and the device id together
        FFKey    £ºadvanced parameter sub-checksum parmater
    \return:
        true or false, if the FFKey is equal to the UserKey which is calculated
        by pDevicedId and algrithom.
*/
static bool_t ffcheck_Advanced(const u32 *pDeviceId, u32 FFKey)
{
    u32  UserKey = 0x0;
    u16 ClcltdKeyFrst, ClcltdKeyScnd;

    ClcltdKeyFrst = calcFrst(pDeviceId, pFK_1);
    ClcltdKeyScnd = calcScnd(pDeviceId, pFK_2);
    UserKey += ClcltdKeyScnd;
    UserKey += ((u32)ClcltdKeyFrst) << 16;
    if (UserKey == FFKey)
    {
        return true;
    }
    else
    {
        return false;
    }
}


/*\brief clear the advanced buffer
    \param in:
        no
    \return:
        no
*/
static void ffcheck_ClrBuffer()
{
    u8 i;
    for (i = 0; i < PARA_ADV_DWORD; i++)
    {
        sAdvancedBuffer[i] = 0;
    }
    uAdvBufferFlag = 0;
    return;
}


/* \brief disable the alert enalbe when the related alert parameter is not
    writeable.
    \param in:
        p_block_instance: pointer to function block

    \return:
        FF error
*/
fferr_t ffcheck_AlertEnableChange(T_FBIF_BLOCK_INSTANCE * p_block_instance)
{
    fferr_t fferr = E_OK;
    //T_FBIF_PTB *p_PTB = p_block_instance->p_block_desc->p_block;

    T_FBS_WRITE_PARAM_LOCAL mp_write;
    //T_FBIF_WRITE_DATA mp_write;
    u8 i, temp = 0;
    bool_t bBit;

    for(i = 0; i < NELEM(ptb_param_readonly); i ++)
    {
        //if the alert related parameter is not writeable, disable the
        //alert enable.
        MN_ASSERT(ptb_param_readonly[i].ptb_index < USIGNED_CHAR_NUMBER);
        bBit = ffcheck_WriteBitmap((u8)ptb_param_readonly[i].ptb_index);
        if (!bBit)
        {
            if (ptb_param_readonly[i].ptb_index != REL_IDX_PTB_IP_DRIVE_CURRENT_HI_ALERT
            && ptb_param_readonly[i].ptb_index != REL_IDX_PTB_IP_DRIVE_CURRENT_LO_ALERT)
            {
                mp_write.subindex  = 6;
            }
            else
            {
                mp_write.subindex  = 7;
            }
            MN_DBG_ASSERT(ptb_param_readonly[i].ptb_index <= 255U);
            mp_write.rel_idx       = (u8)ptb_param_readonly[i].ptb_index;
            UNUSED_OK(ptb_param_readonly[i].alert_offset);
            //mp_write.dest          =
            //    (u8 *)((u8 *)p_PTB + ptb_param_readonly[i].alert_offset);
            mp_write.length        = sizeof(u8);
            mp_write.source        = (u8 *)&temp;
            mp_write.startup_sync        = TRUE;
            //mp_write.phase         = POST_WRITE_ACTION;
            fferr = mn_fbs_write_param_loc(p_block_instance,&mp_write);
        }
    }
    return fferr;
}

/* \brief write advanced parameter
    \param in:
        p_block_instance: pointer to function block
        p_write:  pointer to write resource data

    \return:
        FF error
*/
fferr_t ffcheck_WriteAdvanced
(
    T_FBIF_BLOCK_INSTANCE * p_block_instance,
    T_FBIF_WRITE_DATA *p_write
)
{
    fferr_t fferr = E_OK;
    u32 uDeviceId[CAL_TOTAL_DEVICE_ID_DWORD] = PRAMETER_STANDARD_DEFAULT;
    STRINGV pd_tag[DEVICE_ID_BYTES], dev_id[DEVICE_ID_BYTES];
    u8 dev_addr, i, state;
    T_FBIF_PTB *p_PTB = p_block_instance->p_block_desc->p_block;
    //got the device id.
    get_sm_config_data(&dev_addr, pd_tag, dev_id);

    switch (p_write->subindex)
    {
    case 0:
        state = state_CommitAdvanced;
        break;

    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
        state = state_WriteOne;
        break;

    case 13:
        state = state_CommitBuffer;
        break;

    case 14:
    default:
        state = state_DoNothing;
        break;
    }

    switch (state)
    {
    case state_CommitAdvanced:
        mn_memcpy(uDeviceId, p_write->source, PARA_ADV_REDWRT_BYTES);
        mn_memcpy(&uDeviceId[PARA_ADV_RWF_DWORD], (void *)dev_id, DEVICE_ID_BYTES);

        if (ffcheck_Advanced(uDeviceId,
            mn_pull_u32((void *)(&p_write->source[PARA_ADV_REDWRT_BYTES]))))
        {
            ffcheck_ClrBuffer();
            fferr = ffcheck_AlertEnableChange(p_block_instance);
        }
        else
        {
            fferr = E_FB_PARA_LIMIT;
        }
        break;
    case state_WriteOne:
        //save the write value to the arrays.
        mn_memcpy(&sAdvancedBuffer[p_write->subindex - 1],
                  p_write->source, PARA_BYTE_IN_DWORD);
        mn_SetBufferFlag(p_write->subindex - 1);
        fferr = E_FB_PARA_LIMIT;
        break;

    case state_CommitBuffer:
        //save the write value to the arrays.
        mn_memcpy(&sAdvancedBuffer[p_write->subindex - 1],
                  p_write->source, PARA_BYTE_IN_DWORD);
        mn_SetBufferFlag(p_write->subindex - 1);
        //maybe we just change part of the parameter
        for (i = 0; i < p_write->subindex; i++)
        {
            //if buffer is not initialized, copy it from PTB.
            if (!mn_GetBufferFlag(i))
            {
                sAdvancedBuffer[i] = p_PTB->advanced[i];
            }
        }

        //if we want to set the device to advanced or standard type,
        //check the key.we will change the parameter only when the key
        //is correct.
        mn_memcpy(uDeviceId, sAdvancedBuffer, PARA_ADV_REDWRT_BYTES);
        mn_memcpy(&uDeviceId[PARA_ADV_RWF_DWORD], (void *)dev_id, DEVICE_ID_BYTES);
        if (ffcheck_Advanced(uDeviceId, sAdvancedBuffer[PARA_ADV_RWF_DWORD]))
        {
            p_write->subindex = 0;
            p_write->length = PARA_ADV_REWRCHE_BYTES;

            mn_memcpy((void *)(&p_PTB->advanced[0]),
                (u8*)sAdvancedBuffer, PARA_ADV_REWRCHE_BYTES);
            p_write->source = (u8*)p_PTB->advanced;
            p_write->dest = (u8 *)p_PTB->advanced;
            fferr = ffcheck_AlertEnableChange(p_block_instance);

        }
        else
        {
            fferr = E_FB_PARA_LIMIT;
        }
        ffcheck_ClrBuffer();
        break;

    case state_DoNothing:
    default:
        break;
    }

    return fferr;
}


/* \brief check if the parameter is readeable or not accroding to the advanced
    parameter read bitmap.
   \param in:
        index: the parameter index number

   \return:
        True:  the related parameter is readable
        False: the related parameter is not readable
*/
bool_t ffcheck_ReadBitmap(u8 index)
{
    T_FBIF_PTB *p_PTB;
    bool_t bBit;

    p_PTB = fbs_get_block_inst(ID_PTB_1)->p_block_desc->p_block;

    //check if the parameter is readable or not according to bitmap.
    bBit = mn_GetBit(index,
       (void*)p_PTB->advanced, PARA_ADV_READ_BYTES);

    return bBit;

}

/* \brief check if the parameter is writeeable or not accroding to the advanced
    parameter write bitmap.
   \param in:
        index: the parameter index number

   \return:
        True:  the related parameter is writeable
        False: the related parameter is not writeable
*/
bool_t ffcheck_WriteBitmap(u8 index)
{
    T_FBIF_PTB *p_PTB;
    bool_t bBit;

    p_PTB = fbs_get_block_inst(ID_PTB_1)->p_block_desc->p_block;

    //check if the parameter is writeable or not according to bitmap.
    bBit = mn_GetBit(index,
       (void*)&p_PTB->advanced[PARA_ADV_READ_DWORD], PARA_ADV_READ_BYTES);

    return bBit;

}

/* \brief check if the device is standard type, disable the advanced parameter
    write function, and will reject the write operation.
   \param in:
        p_block_instance: pointer to function block
        p_write:  pointer to write resource data

   \return:
        FF error
*/
fferr_t ffcheck_WriteFilter
(
    const T_FBIF_BLOCK_INSTANCE *p_block_instance,
    const T_FBIF_WRITE_DATA     *p_write
)
{
    fferr_t fferr = E_OK;
    T_FBIF_PTB *p_PTB = p_block_instance->p_block_desc->p_block;

    bool_t bWBit;

    //filter for not writeable parameters.
    MN_ASSERT(p_write->rel_idx < USIGNED_CHAR_NUMBER);
    bWBit = ffcheck_WriteBitmap((u8)p_write->rel_idx);
    if ((!bWBit) && (p_write->rel_idx != REL_IDX_PTB_ADVANCED) )
    {
        fferr = E_FB_WRITE_LOCK;

    }

    //check when the TB.actual mode is LO,reject the parameter write except the
    //MODE_BLK,APP_MODE,Factory_USE1 and Advanced key;
    if (E_OK == fferr)
    {
        if (MODE_LO == p_PTB->mode_blk.actual)
        {
            if (p_write->rel_idx != REL_IDX_PTB_PTB
                &&p_write->rel_idx != REL_IDX_PTB_MODE_BLK
                && p_write->rel_idx != REL_IDX_PTB_APP_MODE
                && p_write->rel_idx != REL_IDX_PTB_ADVANCED
                && p_write->rel_idx != REL_IDX_PTB_FACTORY_USE_1)
            {
                fferr = E_FB_WRONG_MODE;
            }
        }
    }

    return fferr;
}

/* \brief when the parameter is not readable, change all the sub-parameter to 0.
   \param in:
        p_read:  Pointer to a read description block.

   \return:
        no
*/
void ffcheck_ReadFilter
(
    const T_FBIF_BLOCK_INSTANCE * p_block_instance,
    const T_FBIF_READ_DATA *p_read
)
{
    //T_FBIF_WRITE_DATA p_write;
    void *dest;
    T_FBIF_PTB *p_PTB = p_block_instance->p_block_desc->p_block;

    dest = (void*)((u8 *)p_PTB + ptb_param_dir[p_read->rel_idx].par_offset);

    //clean the parameter value in PTB.
    mn_memset(dest, 0, ptb_param_dir[p_read->rel_idx].par_size);

    return;
}

/* This line marks the end of the source */
