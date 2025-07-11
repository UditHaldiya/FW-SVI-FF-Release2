/*
Copyright 2004-2007 by Dresser, Inc., as an unpublished trade secret.  All rights reserved.

This document and all information herein are the property of Dresser, Inc.
It and its contents are  confidential and have been provided as a part of
a confidential relationship.  As such, no part of it may be made public,
decompiled, reverse engineered or copied and it is subject to return upon
demand.
*/
/**
    \file hart.h
    \brief Header for HART Data Link protocol - frame level processing

    CPU: Any

    OWNER: AK

    $Author: Arkkhasin $

    \ingroup HARTDLL
*/
#ifndef HART_H
#define HART_H
#include "errcodes.h"
#include "dimensions.h"
#include "oswrap.h"

#include "hartvers.h"

#ifdef OLD_NVRAM
#include "mncbtypes.h"
#endif
#ifdef COMM_NUM_CHANNELS
#define HART_NUM_CHANNELS COMM_NUM_CHANNELS
#else
#define HART_NUM_CHANNELS 1
#endif

#define HART_MAIN_CHANNEL 0
CONST_ASSERT(HART_MAIN_CHANNEL < HART_NUM_CHANNELS);

#if HART_NUM_CHANNELS >=3
    #define HART_IPC_CHANNEL 1
    #define HART_ISP_CHANNEL 2
    extern const u8 comm_channel_map[HART_NUM_CHANNELS];
    #define hart2uartMap(hart_channel) (comm_channel_map[hart_channel])
#else
    #define hart2uartMap(hart_channel) 0 //one and the only UART
#endif

#if HART_NUM_CHANNELS > 1
extern u8 uart_GetHartTaskAndChannel(u8 uartNo, taskid_t *pTaskId, bool_t *pIsBlockDevice);
#else
MN_INLINE u8 uart_GetHartTaskAndChannel(u8 uartNo, taskid_t *pTaskId, bool_t *pIsBlockDevice)
{
    UNUSED_OK(uartNo);
    if(pTaskId != NULL)
    {
        *pTaskId = TASKID_HART;
    }
    if(pIsBlockDevice != NULL)
    {
        *pIsBlockDevice = false;
    }
    return HART_MAIN_CHANNEL;
}
#endif

//Required interface; at the minimum, handles gap timer
extern u8 HART_timehandler(u8_least hart_channel);


#define HART_NUM_STATUS_BYTES 2 //!< the number of mandatory status bytes stuffed in a HART response

#define HART_OK  0

#define HRT_ZERO                (u8)0
#define HRT_ONE                 (u8)1
#define HRT_MAX_RESPPREAM       (u8)20
#define HRT_MIN_RESPPREAM       (u8)5

#define NBR_REQUEST_PREAM  (u8)5     /* Anzahl der Requestpreambles      */
#define UNIV_CMD_REV       (u8)0x05  /* Revisionstaende fuer universal-, */
#define FLAGS              (u8)0x00

#define ON  1
#define OFF 0

#define FRM_MASK        0x07U    /* The low 3 bits of the delimiter ... */
#define ACK_FRAME       0x06U    /*   describe the Frametype            */
#define STX_FRAME       0x02U
#define BRS_FRAME       0x01U
#define LONG_FRAME      0x80U    /* long/short frame */

#define GAP_TIME        0x03	// 15 - 20 milliseconds


//================== Buffers size and layout ===========================
#define MAXBUFLEN       120
#define RX_MAXBUFLEN MAXBUFLEN
#define TX_MAXBUFLEN MAXBUFLEN

typedef struct  long_address_t
{
    u8 mfr_id_t_addr;     /* manufacturer ID/transmitadress */
    u8 mfr_device_code;   /* device type     */
    u8 dev_id_nbr[3];     /* device ID       */
} long_address_t;


#define MAX_PREAMBLES HRT_MAX_RESPPREAM //10 //Max by protocol is 21
/*lint ++flb
    The following two structures map the response frame
    The trick here is that the command, bytecount and
    data fields always occur at the same buffer offset.
    These structures are padded at the beginning to account
    for this alignment.
*/
typedef struct  long_xmt_hdr_t
{
    u8 dummy_exp[MAX_PREAMBLES];
    u8 delimiter;
    long_address_t long_address;
    u8 command;
    u8 bytecount;
    u8 rsp_code_1;
    u8 rsp_code_2;
    u8 response;
} long_xmt_hdr_t;

typedef struct  short_xmt_hdr_t
{
    u8 dummy_exp[MAX_PREAMBLES];
    u8 dummy_addr[4];
    u8 delimiter;
    u8 address;
    u8 command;
    u8 bytecount;
    u8 rsp_code_1;
    u8 rsp_code_2;
    u8 response;
}short_xmt_hdr_t;
CONST_ASSERT(sizeof(short_xmt_hdr_t) == sizeof(long_xmt_hdr_t));

/*
    structure of the incoming (request frame)
*/
typedef struct  receive_hdr_t
{
    u8 delimiter;
    long_address_t long_address;
    u8 expansion[3];
    u8 command;
    u8 bytecount;
    u8 data;
}receive_hdr_t;

//lint --flb


//================== End Buffers size and layout ===========================

extern void Hart_init(void);


//Old: extern void Hart_DropFrame(void);
extern void Hart_DropFrameEx1(const u8 *dst); //deprecated
#define Hart_DropFrame() (Hart_DropFrameEx1(dst))  //deprecated


extern void maintain_timers(void);

//HART configuration data
/* Hart related configuration data */
#define HART_DATE_LENGTH 3
#define HART_MESSAGE_LENGTH 32
#define HART_DESCRIPTOR_LENGTH 16
#define HART_TAG_LENGTH 8
#define HART_FINAL_LENGTH 3
#define HART_ID_LENGTH 3

typedef u8 pascii_t; //!< HART-packed ascii: 4 characters in 3 bytes
#define HPACKED(size) ((3*(size))/4) //!< Hart-packed ascii size

typedef struct HartData_t
{
    u8 hart_version;
    u8 device_id[HART_ID_LENGTH];		/* device ID number */
	pascii_t message[HPACKED(HART_MESSAGE_LENGTH)];			/* message 32 characters , 24 packed*/
	pascii_t tag[HPACKED(HART_TAG_LENGTH)];			/* tag (8 characters, 6 packed) */
	pascii_t descriptor[HPACKED(HART_DESCRIPTOR_LENGTH)];	/* descriptor (16 characters, 12 packed) */
	u8 date[HART_DATE_LENGTH];			/* date */
	u8 finalass_nbr[HART_FINAL_LENGTH];  	        /* final assembly number */
	u8 transmit_address;	        /* transmitter address */
	u8 nbr_resp_pream;		/* number of response preambles */
	u8 burst_mode_cmd;		/* burst mode command number */
	u8 burst_mode_ctrl;	        /* burst mode control */
    u8 Reserved; //used to be optional LoopCurrentMode; //Per cmd6, it could be none NVRAM variable
	u16 CheckWord;	
} HartData_t;

LINT_PURE(hart_GetHartData)
extern const HartData_t     *hart_GetHartData(void);
SAFEGET(hart_GetAndTestHartData, HartData_t);
SAFESET(hart_SetHartData, HartData_t);



extern void hart_rcv_ok(u8 uartNo, const u8 *buf, u8 flags);
extern void hart_xmit_ok(taskid_t taskid);

LINT_PURE(hart_MsgFromSecondaryMaster)
extern bool_t hart_MsgFromSecondaryMaster(const void *rx_data);

extern void hart_TskHart (void *p_arg); //continuation of main sequencer, not an independent thread

extern void hart_ResetDeviceOnSend(void); //required interface to be provided by low-level code

extern bool_t hart_IsFrameReceived(u8 hart_channel);
extern void hart_ClearFrameReceived(u8 hart_channel);

//Helper functions
#if HART_NUM_CHANNELS > 1
extern void command(u8 hart_channel, const HartData_t *hdata);
extern void hart_DoResetRequest(taskid_t taskid);
#else
//Those functions are static in the source file
#endif


#ifdef OLD_NVRAM
extern void                 hart_SaveHartData(void);
extern void                 hart_InitHartData(InitType_t itype);
#endif //OLD_NVRAM

#endif

/* This line marks the end of the source */
