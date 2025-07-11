/*
Copyright 2013 by Dresser, Inc., as an unpublished trade secret.  All rights reserved.

This document and all information herein are the property of Dresser, Inc.
It and its contents are  confidential and have been provided as a part of
a confidential relationship.  As such, no part of it may be made public,
decompiled, reverse engineered or copied and it is subject to return upon
demand.
*/
/**
    \file mnctllimff.c
    \brief Handling of find stops interface

    CPU: Any

    OWNER: AK
*/


//Softing headers first
#include <softing_base.h>
#include <ptb.h>

#define MODULE_ID (MOD_APPL_TRN | COMP_PAPP)
#define DIAGNOSTIC_RUNNING ((u8)100u)
#define DIAGNOSTIC_NOT_RUNNING ((u8)0u)

//MN FIRMWARE headers second
#include "hartdef.h"
#include "bufutils.h"
#include "process.h"
#include "hartcmd.h"
#include "faultpublic.h"
#include "bufferhandler.h"
#include "diagnostics.h"
#include "vertranpub.h"

//Glue headers last
#include "mndiagprocff.h"
#include "mn_advanced.h"
#define MN_DIAG_SIGNATURE_IDX        0u
#define MN_DIAG_RAMP_TEST_IDX        1u
#define MN_DIAG_SAVE_SIG_BAS_IDX     2u
#define MN_DIAG_SAVE_SIG_CUS_IDX     3u
#define MN_DIAG_SAVE_SIG_LASTS_IDX   4u
#define MN_DIAG_PREP_BAS_READ_IDX    5u
#define MN_DIAG_PREP_CUS_READ_IDX    6u
#define MN_DIAG_PREP_LASTS_READ_IDX  7u
#define MN_DIAG_STEP_TEST_IDX        8u
//#define MN_DIAG_SAVE_SIG_FAC_IDX     9u
#define MN_DIAG_CANCEL_IDX           10u
#define MN_DIAG_DATAHOG_STOP_IDX     11u
#define MN_DIAG_DATAHOG_START_IDX    12u

#define MN_DIAG_FUNC_IDX_OF_AD       10u
#define MN_DIAG_FUNC_LEN_IN_BYTE     4u


//static u16 diag_type = UINT16_MAX; //indicate that we don't know the type

fferr_t ffdiag_ReadDiagnosticCommand(T_FBIF_BLOCK_INSTANCE *p_block_instance, T_FBIF_READ_DATA *p_read, void *from, void *to)
{
    UNUSED_OK(p_read);
    T_FBIF_PTB *p_PTB = p_block_instance->p_block_desc->p_block;
    fferr_t fferr = mn_HART_acyc_cmd(251, from, 0, to);
    if(fferr == E_OK)
    {
        Rsp_CheckProcess_t *d = (void *)((u8 *)to + 2);
        u8 procid = util_GetU8(d->ProcessId[0]);

        //suppress indication of non-diagnostic processes
        if((procid == PROC_FIND_STOPS) || (procid == PROC_AUTOTUNE))
        {
            procid = PROC_NONE;
        }

        p_PTB->offline_diagnostic = (procid==PROC_NONE)?DIAGNOSTIC_NOT_RUNNING:DIAGNOSTIC_RUNNING;
    }

    return fferr;
} //lint !e818 const-ness may be confusing to reader since the pointed-to object is modified indirectly

#if 1
#define DIAG_DATAHOG_START 98
#define DIAG_DATAHOG_STOP 97
#define DIAG_COMMIT_BUFFER_TO_NVMEM NVXLATE_DIAGTYPE_WRITE //96
//reserved for now #define DIAG_WRITE_BUFFER 95
#define DIAG_ENABLE_WRITE_BUFFER 94
#define DIAG_READ_NVMEM_TO_BUFFER NVXLATE_DIAGTYPE_READ //93
#endif

fferr_t ffdiag_WriteDiagnosticCommand(T_FBIF_BLOCK_INSTANCE *p_block_instance, T_FBIF_WRITE_DATA *p_write, void *from, void *to)
{
    fferr_t fferr = E_OK;
    u8 cmd=0;
    T_FBIF_PTB *p_PTB = p_block_instance->p_block_desc->p_block;
    _DIAGNOSTIC_CONFIGURATION *conf = &p_PTB->diagnostic_configuration;

    bool_t bBit = false, bBit1 = false, bBit2 = false;
    u8 selection; //now u8
    u8 len = 0;
    memcpy(&selection, p_write->source, sizeof(selection));

    Req_RunaProcess_t *subcmd = from;
    /*if(MODE_MAN != p_PTB->mode_blk.actual)
    {
        fferr = E_FB_WRONG_MODE;
    }
    else
    {*/
        switch(selection)
        {
#if 1
            case DIAG_DATAHOG_START:
            case DIAG_DATAHOG_STOP:
            {
                //check if the parameter is enabled or not according to
                //function user key in advanced parameter.
                bBit = mn_GetBit(MN_DIAG_DATAHOG_STOP_IDX,
                (void*)&p_PTB->advanced[MN_DIAG_FUNC_IDX_OF_AD], MN_DIAG_FUNC_LEN_IN_BYTE);
                bBit1= mn_GetBit(MN_DIAG_DATAHOG_START_IDX,
                (void*)&p_PTB->advanced[MN_DIAG_FUNC_IDX_OF_AD], MN_DIAG_FUNC_LEN_IN_BYTE);
                //if any bit is enable, run the program
                if (bBit || bBit1)
                {
                    Req_DataCollection_t *s = (void *)((u8 *)from + MN_OFFSETOF(Req_RunaProcess_t, endmark));
                    util_PutU16(s->DataCollectionSkipCount[0], conf->sampling_time);
                    util_PutU8(s->DataCollectionBitmap[0], conf->option);
                    DataCollectionCommandEnum_t c;
                    if(selection==DIAG_DATAHOG_START)
                    {
                        c = HC_StartCollecting;
                    }
                    else //selection==DIAG_DATAHOG_STOP
                    {
                        c = HC_StopCollecting;
                    }
                    util_PutU8(s->DataCollectionCommand[0], (u8)c);
                    util_PutU8(s->BufferId[0], DIAGBUF_DEFAULT);
                    len = MN_OFFSETOF(Req_RunaProcess_t, endmark) + sizeof(Req_DataCollection_t);
                    cmd = 22;
                }
                else
                {
                    fferr = E_FB_WRITE_LOCK;
                }
                break;
            }
            case DIAG_ENABLE_WRITE_BUFFER:
            {
                Req_EnableExternalWritesToDiagnosticBuffer_t *s = (void *)((u8 *)from + MN_OFFSETOF(Req_RunaProcess_t, endmark));
                len = MN_OFFSETOF(Req_RunaProcess_t, endmark) + sizeof(Req_EnableExternalWritesToDiagnosticBuffer_t);
                util_PutU8(s->BufferId[0], 0);
                cmd = 193;
                break;
            }
            case DIAG_READ_NVMEM_TO_BUFFER:
            {
                Req_ReadNVMEMToDiagnosticBuffer_t *s = (void *)((u8 *)from + MN_OFFSETOF(Req_RunaProcess_t, endmark));
                util_PutU8(s->BufferId[0], 0);
                util_PutU8(s->NVMEMbank[0], conf->direction);
                len = MN_OFFSETOF(Req_RunaProcess_t, endmark) + sizeof(Req_ReadNVMEMToDiagnosticBuffer_t);
                cmd = 194;
                break;
            }
            case DIAG_COMMIT_BUFFER_TO_NVMEM:
            {
                Req_CommitNVMEMImageFromDiagnosticBuffer_t *s = (void *)((u8 *)from + MN_OFFSETOF(Req_RunaProcess_t, endmark));
                util_PutU8(s->BufferId[0], 0);
                util_PutU8(s->NVMEMbank[0], conf->direction);
                len = MN_OFFSETOF(Req_RunaProcess_t, endmark) + sizeof(Req_CommitNVMEMImageFromDiagnosticBuffer_t);
                cmd = 192;
                break;
            }
#endif
            case DIAG_EXECUTE_STEP_TEST:
                {
                    if(MODE_MAN != p_PTB->mode_blk.actual)
                    {
                        fferr = E_FB_WRONG_MODE;
                    }
                    else
                    {
                        //check if the parameter is enabled or not according to
                        //function uer key in advanced parameter.
                        bBit = mn_GetBit(MN_DIAG_STEP_TEST_IDX,
                        (void*)&p_PTB->advanced[MN_DIAG_FUNC_IDX_OF_AD], MN_DIAG_FUNC_LEN_IN_BYTE);
                        if (bBit)
                        {
                            //Populate the command buffer
                            Req_StepTest_t *s = (void *)((u8 *)from + MN_OFFSETOF(Req_RunaProcess_t, endmark));
                            len = MN_OFFSETOF(Req_RunaProcess_t, endmark) + sizeof(Req_StepTest_t);
                            util_PutFloat(s->StartPosition[0], conf->start_postion);
                            util_PutFloat(s->EndPosition[0], conf->end_position);
                            util_PutU16(s->SamplingTime[0], conf->sampling_time); //sample time
                            cmd = 3;
                        }
                        else
                        {
                            fferr = E_FB_WRITE_LOCK;
                        }
                    }
                }
                break;
            case DIAG_EXEC_SIGNATURE:
                {
                    if(MODE_MAN != p_PTB->mode_blk.actual)
                    {
                        fferr = E_FB_WRONG_MODE;
                    }
                    else
                    {
                        //check if the parameter is enabled or not according to
                        //function uer key in advanced parameter.
                        bBit = mn_GetBit(MN_DIAG_SIGNATURE_IDX,
                        (void*)&p_PTB->advanced[MN_DIAG_FUNC_IDX_OF_AD], MN_DIAG_FUNC_LEN_IN_BYTE);
                        if (bBit)
                        {
                            //Demonstrate that encoding matches between FFP and APP
                            CONST_ASSERT(HC_DIAGCTL_OPENLOOP==DIAG_OPTION_OPEN_LOOP);
                            CONST_ASSERT(HC_DIAGCTL_CLOSEDLOOP==DIAG_OPTION_CLOSED_LOOP);
                            CONST_ASSERT(DIAG_DIRECTION_BOTH==HC_DIAGDIR_UPDOWN);
                            CONST_ASSERT(DIAG_DIRECTION_ONE==DIAGDIR_ONEWAY);

                            //Populate the command buffer
                            Req_ExtendedActuatorDiagnostics_t *s = (void *)((u8 *)from + MN_OFFSETOF(Req_RunaProcess_t, endmark));
                            len = MN_OFFSETOF(Req_RunaProcess_t, endmark) + sizeof(Req_ExtendedActuatorDiagnostics_t);
                            util_PutFloat(s->StartPosition[0], conf->start_postion);
                            util_PutFloat(s->EndPosition[0], conf->end_position);
                            util_PutFloat(s->SetpointRampSpeed[0], conf->set_point_rate);
                            util_PutU8(s->DiagDirection[0], conf->direction);
                            util_PutU8(s->DiagControlOption[0], conf->option);
                            cmd = 2;
                        }
                        else
                        {
                            fferr = E_FB_WRITE_LOCK;
                        }
                    }
                }
                break;
            case DIAG_EXEC_RAMP_TEST:
                {
                    if(MODE_MAN != p_PTB->mode_blk.actual)
                    {
                        fferr = E_FB_WRONG_MODE;
                    }
                    else
                    {
                        //check if the parameter is enabled or not according to
                        //function uer key in advanced parameter.
                        bBit = mn_GetBit(MN_DIAG_RAMP_TEST_IDX,
                        (void*)&p_PTB->advanced[MN_DIAG_FUNC_IDX_OF_AD], MN_DIAG_FUNC_LEN_IN_BYTE);
                        if (bBit)
                        {
                            //Demonstrate that encoding matches between FFP and APP
                            CONST_ASSERT(DIAG_DIRECTION_BOTH==HC_DIAGDIR_UPDOWN);
                            CONST_ASSERT(DIAG_DIRECTION_ONE==DIAGDIR_ONEWAY);

                            //Populate the command buffer
                            Req_RampTest_t *s = (void *)((u8 *)from + MN_OFFSETOF(Req_RunaProcess_t, endmark));
                            len = MN_OFFSETOF(Req_RunaProcess_t, endmark) + sizeof(Req_RampTest_t);
                            util_PutFloat(s->StartPosition[0], conf->start_postion);
                            util_PutFloat(s->EndPosition[0], conf->end_position);
                            util_PutFloat(s->SetpointRampSpeed[0], conf->set_point_rate);
                            util_PutU8(s->DiagDirection[0], conf->direction);
                            cmd = 4;
                        }
                        else
                        {
                            fferr = E_FB_WRITE_LOCK;
                        }
                    }
                }
                break;
            case DIAG_SAVE_AS_BASELINE:
            case DIAG_SAVE_AS_CUSTOM:
            case DIAG_SAVE_AS_CURRENT:
                {
                    if(MODE_MAN != p_PTB->mode_blk.actual)
                    {
                        fferr = E_FB_WRONG_MODE;
                    }
                    else
                    {
                        //check if the parameter is enabled or not according to
                        //function uer key in advanced parameter.
                        bBit = mn_GetBit(MN_DIAG_SAVE_SIG_BAS_IDX,
                        (void*)&p_PTB->advanced[MN_DIAG_FUNC_IDX_OF_AD], MN_DIAG_FUNC_LEN_IN_BYTE);
                        bBit1= mn_GetBit(MN_DIAG_SAVE_SIG_CUS_IDX,
                        (void*)&p_PTB->advanced[MN_DIAG_FUNC_IDX_OF_AD], MN_DIAG_FUNC_LEN_IN_BYTE);
                        bBit2= mn_GetBit(MN_DIAG_SAVE_SIG_LASTS_IDX,
                        (void*)&p_PTB->advanced[MN_DIAG_FUNC_IDX_OF_AD], MN_DIAG_FUNC_LEN_IN_BYTE);
                        //if any bit is enable, run the program
                        if (bBit || bBit1 || bBit2)
                        {
                            //Populate the command buffer
                            Req_SaveDiagnosticSignature_t *s = (void *)((u8 *)from + MN_OFFSETOF(Req_RunaProcess_t, endmark));
                            len = MN_OFFSETOF(Req_RunaProcess_t, endmark) + sizeof(Req_SaveDiagnosticSignature_t);
                            util_PutU8(s->SignatureType[0], HC_DIAGRW_EXT_SIGNATURE);
                            u8 SignatureAssignment;
                            if(selection == DIAG_SAVE_AS_BASELINE)
                            {
                                SignatureAssignment = HC_DIAGRW_BASELINE;
                            }
                            else if(selection == DIAG_SAVE_AS_CUSTOM)
                            {
                                SignatureAssignment = HC_DIAGRW_USER;
                            }
                            else
                            {
                                SignatureAssignment = HC_DIAGRW_CURRENT;
                            }
                            util_PutU8(s->SignatureAssignment[0], SignatureAssignment);
                            cmd = 20;
                        }
                        else
                        {
                            fferr = E_FB_WRITE_LOCK;
                        }
                    }
                }
                break;
            case DIAG_PREPARE_BASELINE_FOR_UPLOAD:
            case DIAG_PREPARE_CUSTOM_FOR_UPLOAD:
            case DIAG_PREPARE_CURRENT_FOR_UPLOAD:
                {
                    if((MODE_MAN != p_PTB->mode_blk.actual) &&
                       (MODE_AUTO != p_PTB->mode_blk.actual))
                    {
                        fferr = E_FB_WRONG_MODE;
                    }
                    else
                    {
                        //check if the parameter is enabled or not according to
                        //function uer key in advanced parameter.
                        bBit = mn_GetBit(MN_DIAG_PREP_BAS_READ_IDX,
                        (void*)&p_PTB->advanced[MN_DIAG_FUNC_IDX_OF_AD], MN_DIAG_FUNC_LEN_IN_BYTE);
                        bBit1= mn_GetBit(MN_DIAG_PREP_CUS_READ_IDX,
                        (void*)&p_PTB->advanced[MN_DIAG_FUNC_IDX_OF_AD], MN_DIAG_FUNC_LEN_IN_BYTE);
                        bBit2= mn_GetBit(MN_DIAG_PREP_LASTS_READ_IDX,
                        (void*)&p_PTB->advanced[MN_DIAG_FUNC_IDX_OF_AD], MN_DIAG_FUNC_LEN_IN_BYTE);
                        //if any bit is enable, run the program
                        if (bBit || bBit1 || bBit2)
                        {
                            //Populate the command buffer
                            Req_ReadDiagnosticSignatureIntoDiagnosticBuffer_t *s = (void *)((u8 *)from + MN_OFFSETOF(Req_RunaProcess_t, endmark));
                            len = MN_OFFSETOF(Req_RunaProcess_t, endmark) + sizeof(Req_ReadDiagnosticSignatureIntoDiagnosticBuffer_t);
                            util_PutU8(s->SignatureType[0], HC_DIAGRW_EXT_SIGNATURE);
                            u8 SignatureAssignment;
                            if(selection == DIAG_PREPARE_BASELINE_FOR_UPLOAD)
                            {
                                SignatureAssignment = HC_DIAGRW_BASELINE;
                            }
                            else if(selection == DIAG_PREPARE_CUSTOM_FOR_UPLOAD)
                            {
                                SignatureAssignment = HC_DIAGRW_USER;
                            }
                            else
                            {
                                SignatureAssignment = HC_DIAGRW_CURRENT;
                            }
                            util_PutU8(s->SignatureAssignment[0], SignatureAssignment);
                            util_PutU8(s->FileVersion[0], 0); //we don't support history FIFO (yet?)
                            util_PutU8(s->BufferId[0], DIAGBUF_DEFAULT); //we don't support reading into other buffer(s) (yet?)
                            cmd = 21;
                        }
                        else
                        {
                            fferr = E_FB_WRITE_LOCK;
                        }
                    }
                }
                break;
            case DIAG_CANCEL_TEST:
                {
                    if(MODE_MAN != p_PTB->mode_blk.actual)
                    {
                        fferr = E_FB_WRONG_MODE;
                    }
                    else
                    {
                        //check if the parameter is enabled or not according to
                        //function uer key in advanced parameter.
                        bBit = mn_GetBit(MN_DIAG_CANCEL_IDX,
                        (void*)&p_PTB->advanced[MN_DIAG_FUNC_IDX_OF_AD], MN_DIAG_FUNC_LEN_IN_BYTE);
                        if (bBit)
                        {
                            len = 0;
                            fferr = mn_HART_acyc_cmd(253, from, len, to);
                        }
                        else
                        {
                            fferr = E_FB_WRITE_LOCK;
                        }
                    }
                }
                break;
            default:
                {
                    if(MODE_MAN != p_PTB->mode_blk.actual)
                    {
                        fferr = E_FB_WRONG_MODE;
                    }
                    else
                    {
                        fferr = E_FB_PARA_CHECK;
                    }
                }
                break;
        }
        //call Cmd 190 only if not CANCEL (if CANCEL, Cmd 253 is already called inside switch)
        if ( (fferr == E_OK) && (selection != DIAG_CANCEL_TEST) )
        {
            util_PutU8(subcmd->SubCmdNum[0], cmd);
            fferr = mn_HART_acyc_cmd(190, from, len, to);
        }
#if 0
        if(fferr == E_OK)
        {
            diag_type = UINT16_MAX; //indicate that we don't know the type we are getting
        }
#endif
    //}

    return fferr;
} //lint !e818 const-ness may be confusing to reader since the pointed-to object is modified indirectly

//---------- Results read part ------------------------

fferr_t ffdiag_ReadDiagnosticData(T_FBIF_BLOCK_INSTANCE *p_block_instance, T_FBIF_READ_DATA *p_read, void *from, void *to)
{
    if(p_read->subindex > 1)
    {
        //we read a member individually; whatever is stored there now; don't deeply care
        return E_OK;
    }

    //The real thing: pull the data piece from APP and advance the pointer
    T_FBIF_PTB *p_PTB = p_block_instance->p_block_desc->p_block;
    Req_ReadDataBufferRaw_t *s = from;
    Rsp_ReadDataBufferRaw_t *d = (void *)((u8 *)to + 2);

    util_PutU8(s->BufferId[0], DIAGBUF_DEFAULT);
    util_PutU16(s->DataOffset[0], p_PTB->diagnostic_data[0]);

    u8 sample_size; //in bytes
    u8 skip_count = (u8)p_PTB->diagnostic_data[1]; //AK: clamp?
    sample_size = sizeof(dsample_t); //doesn't matter as long as it's positive
    if(p_PTB->diagnostic_data[0]==0) //data offset
    {
        //we are reading the header
        skip_count = 0;
    }

    util_PutU8(s->DataSampleSize[0], sample_size);
    util_PutU8(s->DataSampleSkipCount[0], skip_count);

    fferr_t fferr = mn_HART_acyc_cmd(193, from, MN_OFFSETOF(Rsp_ReadDataBufferRaw_t, endmark), to);
    if(fferr == E_OK)
    {
#if 0
        u16 offset = p_PTB->diagnostic_data[0];
#endif
        p_PTB->diagnostic_data[0] = util_GetU16(d->DataOffset[0]); //data offset
        //p_PTB->diagnostic_data[1] - Skip Count is a pass-through from "source" buffer
        p_PTB->diagnostic_data[2] = util_GetU8(d->DataSampleCount[0]); //Data sample count - 3rd word in the array

        mn_memcpy(&p_PTB->diagnostic_data[3], d->RawDataSamples[0], (sizeof(p_PTB->diagnostic_data[0]))*(NELEM(p_PTB->diagnostic_data) - 3));
        CONST_ASSERT((sizeof(p_PTB->diagnostic_data[0]))*(NELEM(p_PTB->diagnostic_data) - 3) == sizeof(d->RawDataSamples));
#if 0
        if(offset == 0)
        {
            //We just read the header
            diag_type = p_PTB->diagnostic_data[3]; //came with the header by convention
        }
#endif
    }

    return E_OK; //even in case of failure allow a read
} //lint !e818 const-ness may be confusing to reader since the pointed-to object is modified indirectly

fferr_t ffdiag_WriteDiagnosticData(const T_FBIF_BLOCK_INSTANCE *p_block_instance, T_FBIF_WRITE_DATA *p_write, void *from, void *to)
{
    UNUSED_OK(p_write);
    fferr_t fferr;
    T_FBIF_PTB *p_PTB = p_block_instance->p_block_desc->p_block;
    if(p_PTB->offline_diagnostic != DIAG_ENABLE_WRITE_BUFFER)
    {
        return E_OK; //nothing to do
    }
    /* EXPLANATION:
    This function MUST be called after the p_PTB->diagnostic_data is populated
    in the Write handler.
    */
    Req_WriteFactoryValues_t *s1 = from;
    util_PutU8(s1->SubCmdNum[0], 193);
    Req_ExternalWriteToDiagnosticBuffer_t *s = (void *)((u8 *)from + MN_OFFSETOF(Req_WriteFactoryValues_t, endmark));
    util_PutU8(s->BufferId[0], DIAGBUF_DEFAULT);
    util_PutU16(s->DataOffset[0], p_PTB->diagnostic_data[0]);
    //p_PTB->diagnostic_data[1] Skip count N/A
    u16 DataSampleCount = p_PTB->diagnostic_data[2];
    if(DataSampleCount > sizeof(s->RawDataSamples))
    {
        fferr = E_FB_PARA_LIMIT;
    }
    else
    {
        util_PutU8(s->DataSampleCount[0], (u8)DataSampleCount);
        CONST_ASSERT( (sizeof(p_PTB->diagnostic_data) - 3*sizeof(p_PTB->diagnostic_data[0])) <= sizeof(s->RawDataSamples));
        mn_memcpy(s->RawDataSamples[0], &p_PTB->diagnostic_data[3], DataSampleCount);
        u8 len = MN_OFFSETOF(Req_WriteFactoryValues_t, endmark) + sizeof(Req_ExternalWriteToDiagnosticBuffer_t);

        fferr = mn_HART_acyc_cmd(130, from, len, to);
    }
    return fferr;
}
/* This line marks the end of the source */
