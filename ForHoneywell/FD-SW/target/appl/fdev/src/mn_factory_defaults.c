/*
Copyright 2012 by Dresser, Inc., as an unpublished trade secret.  All rights reserved.

This document and all information herein are the property of Dresser, Inc.
It and its contents are  confidential and have been provided as a part of
a confidential relationship.  As such, no part of it may be made public,
decompiled, reverse engineered or copied and it is subject to return upon
demand.
*/
/**
    \file mn_facotry_defaults.c
    \brief implement the parameter failed_state read

    CPU: Any

    OWNER: Victoria
    $Date: 04/27/13 14:19p $
*/

//Softing headers first
#include <softing_base.h>
#include <ptb.h>
#include <RTOS.h>

#include "hw_if.h"
#include "hartdef.h"
#include "mnhart2ff.h"
#include "bufutils.h"
#include "mn_factory_defaults.h"
#include "mn_fstate.h"
//#include "uipublic.h"
//#include "uimenued.h"
//#include "hcimport_enum.h"
#include "eep_blk3.h"
#include "eep_if.h"
#include "fbif_dcl.h"
#include "mn_advanced.h"
#include "appl_ptb.h"


#define SAVE_AP_DELAY_TIME_MS    200u

/** \brief Wait for completion of a process in APP
\param snd_buf - working send buffer scratchpad
\param rcv_buf - working receive buffer scratchpad
\return error code (in comm. to APP)
*/
static fferr_t osif_WaitAppProcess(void)
{
    u8 taskid = osif_get_current_task_id();
    fferr_t fferr;
    do
    {
        //Wait until the APP process completes
        osif_TickleWatchdog(taskid);
        OS_Delay(SAVE_AP_DELAY_TIME_MS);
        fferr = mn_HART_acyc_cmd(251, snd_buf, 0, rcv_buf);
        Rsp_CheckProcess_t *rsp = (void *)((u8 *)rcv_buf + 2);
        u8 procid = util_GetU8(rsp->ProcessId[0]);
        if(procid == 0)
        {
            //Completed!
            break;
        }
    } while(fferr == E_OK);
    return fferr;
}


/* \brief reset the APP by hart command 42
   \param in:
        snd_buf:  send buffer
        rcv_buf:  receive buffer
   \return:
        FF error
*/
fferr_t ffres_restart_APP(void)
{
    fferr_t fferr;

    fferr = mn_HART_acyc_cmd(42, snd_buf, HC42_REQ_LENGTH
        + MN_OFFSETOF(Req_Reset_t, endmark), rcv_buf);

    return fferr;
}

/* \brief When the restart parameter choose the "RESTART_FACTORY_DEFAULTS",
    we need to do "RESTART_HIDDEN" at first, if we do not do "hidden", return
    false to prevent the user to do "RESTART_FACTORY_DEFAULTS".
    the value will be 0 from gw file.
   \param in:
        snd_buf:  send buffer
        rcv_buf:  receive buffer
   \return:
        true or false, if the device have done "hidden", return true.
*/
static bool_t ffres_restart_check_hidden_done(void)
{
    T_EEP_RESULT  eep_result;
    MODE         sTestMode;

    //get the mode parameter from blk3;
    eep_result = eep_blk3_get(MN_OFFSETOF(eep_blk3_t, tb_factory_default.ptb_s.mode_blk),
                            elemsize(T_FBIF_PTB,mode_blk), (void*)&sTestMode);
    if (eep_result != EEP_OK)
    {
        return false;
    }
    //if the target and actual mode are all 0, it means we do not do hidden before.
    if (sTestMode.actual == 0 && sTestMode.target == 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

/** \brief When the restart parameter choose the "HIDDEN", we need to
    save the TB block parameters to eep blk3. Then we need to send a
    hart command to save the current APP parameter also.
   \param snd_buf - send buffer
   \param      rcv_buf:  receive buffer
   \return        FF error
*/
fferr_t ffres_restart_hidden(void)
{
    fferr_t fferr;
    T_EEP_RESULT  eep_result;

    //T_FBIF_BLOCK_INSTANCE * p_block_instance = fbs_get_block_inst(ID_PTB_1);
    //T_FBIF_PTB *    p_PTB = p_block_instance->p_block_desc->p_block;

    Req_WriteSettings_t* req = (void *)snd_buf;
    //send hart command 155,6 to set the APP mode to Failsafe.
    util_PutU8(req->SubCmdNum[0], 6);
    fferr = mn_HART_acyc_cmd(155, snd_buf, HC155_REQ_LENGTH +
            MN_OFFSETOF(Req_SetFailedState_t, endmark), rcv_buf);
    if (fferr == E_OK)
    {
        //send hart command 190,130 to save current APP parameters.
        util_PutU8(req->SubCmdNum[0], 130);
        fferr = mn_HART_acyc_cmd(190, snd_buf, HC190_REQ_LENGTH +
            MN_OFFSETOF(Req_SaveNvmemAsFactoryDefault_t, endmark), rcv_buf);
    }
    if (fferr == E_OK)
    {
        //store the PTB structure to the blk3;
        u8 *p_dest;
        eep_result = eep_provide_block (EEP_APPL_BLK3, sizeof(eep_blk3_t), (void**) &p_dest);
        if (eep_result == EEP_OK)
        {
            //Populate the image in blk3
            eep_result = eep_ReadFB(ID_PTB_1,
                                    p_dest + MN_OFFSETOF(eep_blk3_t, tb_factory_default.ptb_s),
                                    sizeof(T_FBIF_PTB_STATIC),
                                    p_dest + MN_OFFSETOF(eep_blk3_t, tb_factory_default.ptb_n),
                                    sizeof(T_FBIF_PTB_NV)
                                        );
        }

        if (eep_result == EEP_OK)
        {
            ///transfer the private data from blk2 eep_blk2_private to
            //blk3 eep_blk3_private_factory_default;
            eep_result = eep_blk23_transfer(MN_OFFSETOF(eep_blk3_t, eep_blk3_private_factory_default),
                MN_OFFSETOF(eep_blk2_t, eep_blk2_private), sizeof(eep_blk2_private_t));
        }

        if (eep_result == EEP_OK)
        {
            eep_put(EEP_APPL_BLK3);
        }

        if (eep_result != EEP_OK)
        {
            fferr = E_FB_EEP_NO_WRITE;
        }
    }

    if(fferr == E_OK)
    {
        //Wait until the APP process completes
        fferr = osif_WaitAppProcess();
    }

    //Clear failsafe (unconditionally)
    util_PutU8(req->SubCmdNum[0], 7);
    fferr_t fferr1 = mn_HART_acyc_cmd(155, snd_buf, HC155_REQ_LENGTH +
        MN_OFFSETOF(Req_ClearFailedState_t, endmark), rcv_buf);

    //but report an error, if any
    if(fferr == E_OK)
    {
        fferr = fferr1;
    }

    return fferr;
}

/** \brief When the restart parameter choose the "RESTART_FACTORY_DEFAULTS",
    we need to restore the TB block parameters from eep blk3. Then we need
    to send a hart command to restore the APP parameter using the saving datas.
   \param in:
        snd_buf:  send buffer
        rcv_buf:  receive buffer
   \return:
        FF error
*/
fferr_t ffres_restart_factory_defaults(void)
{
    u16     uStRev;
    fferr_t fferr;
    T_EEP_RESULT  eep_result;

    T_FBIF_BLOCK_INSTANCE * p_block_instance = fbs_get_block_inst(ID_PTB_1);
    T_FBIF_PTB *    p_PTB = p_block_instance->p_block_desc->p_block;
    Req_WriteSettings_t* req = (void *)&snd_buf[0];
    Req_RestoreFactoryDefaultNvmem_t* req_129 = (void*)((u8*)snd_buf + HC190_REQ_LENGTH);

    //check if the mode is OOS both in TB.target and TB.actual.
    if (p_PTB->mode_blk.target != MODE_OS || p_PTB->mode_blk.actual != MODE_OS)
    {
        fferr = E_FB_WRONG_MODE;
        return fferr;
    }
    //check if we have done "hidden" or not.
    if (!ffres_restart_check_hidden_done())
    {
        fferr = E_FB_PARA_LIMIT;
        return fferr;
    }
    //send hart command 155 to set the APP mode to Failsafe.
    util_PutU8(req->SubCmdNum[0], 6);
    fferr = mn_HART_acyc_cmd(155, snd_buf, HC155_REQ_LENGTH +
            MN_OFFSETOF(Req_SetFailedState_t, endmark), rcv_buf);
    if (fferr == E_OK)
    {
        //send a hart command to resotre current APP parameters.hart cmd 190,129
        util_PutU8(req->SubCmdNum[0], 129);
        util_PutU8(req_129->NvmemDefaultsMaskFlags[0], 4);
        util_PutU8(req_129->NvmemDefaultsMatchFlags[0], 4);
        util_PutU8(req_129->NvmemDefaultsTestOnly[0], 0);
        util_PutU8(req_129->NvmemVolumeId[0], 0);

        fferr = mn_HART_acyc_cmd(190, snd_buf, HC190_REQ_LENGTH +
            MN_OFFSETOF(Req_RestoreFactoryDefaultNvmem_t, endmark), rcv_buf);
    }
    if (fferr != E_OK)
    {
        return fferr;
    }
    // Retrieve the current ST_REV value;
    uStRev = p_PTB->st_rev+1;

    //store the PTB structure to the blk3;
    eep_blk3_t *p_dest;
    eep_result = eep_provide_block (EEP_APPL_BLK3, sizeof(eep_blk3_t), (void **)&p_dest);
    if (eep_result == EEP_OK)
    {
        eep_AcquireMutex();
        p_PTB->st_rev = uStRev;
        eep_ptb_t *pfd = &(p_dest->tb_factory_default);
        pfd->ptb_s.st_rev = uStRev;
        //Populate the image in blk3
        eep_result = eep_SaveFB(ID_PTB_1,
                                &pfd->ptb_s,
                                0,
                                sizeof(T_FBIF_PTB_STATIC),
                                &pfd->ptb_n,
                                0,
                                sizeof(T_FBIF_PTB_NV)
                                    );
        eep_ReleaseMutex();
        if(eep_result != EEP_OK)
        {
            fferr = E_FB_EEP_NO_WRITE;
        }
    }


    //save the tb block static/nv data to NV
    if (fferr != E_OK)
    {
        return fferr;
    }
    //transfer the private data from blk3 eep_blk3_private_factory_default to
    //blk2 eep_blk2_private;
    eep_result = eep_blk32_transfer(MN_OFFSETOF(eep_blk2_t, eep_blk2_private),
        MN_OFFSETOF(eep_blk3_t, eep_blk3_private_factory_default), sizeof(eep_blk2_private_t));

    if (eep_result != EEP_OK)
    {
        return E_FB_EEP_NO_WRITE;
    }

    u32 stamp1 = eep_get_stamp(); //we need to wait until deferred write completes
    //(twice because we may have asked for write in the middle of the cycle

    fferr = osif_WaitAppProcess();

    //First wait for eep cycle to complete
    u8 taskid = osif_get_current_task_id();
    u32 stamp2;
    do
    {
        osif_TickleWatchdog(taskid);
        OS_Delay(SAVE_AP_DELAY_TIME_MS);
        stamp2 = eep_get_stamp(); //watching for stamp1 change
    } while(stamp2 == stamp1);

    //send hart command 155,7 to clear the Failsafe.
    //We probably won't change the mode though. Just clear faults.
    util_PutU8(req->SubCmdNum[0], 7);
    (void)mn_HART_acyc_cmd(155, snd_buf, HC155_REQ_LENGTH +
        MN_OFFSETOF(Req_ClearFailedState_t, endmark), rcv_buf);

    fferr_t fferr1 = mn_HART_acyc_cmd(42, snd_buf, HC42_REQ_LENGTH
        + MN_OFFSETOF(Req_Reset_t, endmark), rcv_buf);
    if (fferr == E_OK)
    {
        fferr = fferr1;
    }

    //Second wait for eep cycle to complete
    do
    {
        osif_TickleWatchdog(taskid);
        OS_Delay(SAVE_AP_DELAY_TIME_MS);
        stamp1 = eep_get_stamp(); //now watching for stamp2 change
    } while(stamp2 == stamp1);

    if (E_OK == fferr)
    {
        //re-check the alert related parameters.
        fferr = ffcheck_AlertEnableChange(p_block_instance);
    }
    // Note: CMD 42 resets only the APP uC, the FF uC still has to be reset
    // Force the Reset on FF uC here.
    if (fferr == E_OK)
    {
        Reset_CPU();
    }

    return fferr;
}

#define LOSE_ADVANCED 0
/*Set to 1 to see how RB:RESTART (a) to defaults and then (b) processor loses Advanced.
It looks like the crime is somewhere in Softing code; I won't investigate it yet.
*/

/** \brief When the restart parameter choose the "RESTART_DEFAULTS",
    we need to restore the advanced parameter in TB block from eep blk3, as
    the value will be 0 from gw file.
   \return     FF error
*/
fferr_t ffres_restart_restore_advanced(void)
{
    fferr_t fferr;
    T_EEP_RESULT  eep_result;

    T_FBIF_BLOCK_INSTANCE * p_block_instance = fbs_get_block_inst(ID_PTB_1);
    T_FBIF_PTB *    p_PTB = p_block_instance->p_block_desc->p_block;

    //get the advanced parameter from blk3;
    if(ffres_restart_check_hidden_done())
    {
        //we have something to restore from
        CONST_ASSERT(elemsize(eep_blk3_t, tb_factory_default.ptb_n.advanced) == elemsize(T_FBIF_PTB,advanced));

	    (void)eep_blk3_get(MN_OFFSETOF(eep_blk3_t, tb_factory_default.ptb_n.advanced),
                                  elemsize(T_FBIF_PTB,advanced), (void*)p_PTB->advanced);
    }

    //Save Advanced from p_PTB->advanced, whether it came from eep block 3 or not

#if LOSE_ADVANCED
    p_PTB->st_rev++;
#endif
    //save the parameter advanced to NV
    eep_result = eep_SaveFB(ID_PTB_1,
#if LOSE_ADVANCED
                            &p_PTB->st_rev,
                            MN_OFFSETOF(T_FBIF_PTB_STATIC, st_rev),
                            elemsize(T_FBIF_PTB,st_rev),
#else
                            NULL, 0, 0,
#endif
                            p_PTB->advanced,
                            MN_OFFSETOF(T_FBIF_PTB_NV, advanced),
                            elemsize(T_FBIF_PTB,advanced));

    if (eep_result != EEP_OK)
    {
        fferr = E_FB_PARA_CHECK; //or E_FB_TEMP_ERROR?
    }
    else
    {
        //re-check the alert related parameters.
        fferr = ffcheck_AlertEnableChange(p_block_instance);
    }

    return fferr;
}

/*This is the last line of this file*/

