#include "mnwrap.h"
#include "datahog.h"
#include "diagnostics.h"
#include "timebase.h"
#include "mnassert.h"
#include "diagrw.h"

#ifdef DATAHOG_POSITION
#include "position.h"
#endif

#if defined(DATAHOG_MAINPRESSURE) || defined(DATAHOG_PILOTPRESSURE) || defined(DATAHOG_SUPPLYPRESSURE) || defined(DATAHOG_PRESSUREB)
#include "pressures.h"
#endif

#ifdef DATAHOG_IPCURRENT
#include "ipcurr.h"
#endif

#ifdef DATAHOG_SETPOINT
#include "devicemode.h"
#include "ctllimits.h"
#endif

typedef s16 DatahogFunc_t(void);

//--------------------- Shims --------------------------
#ifdef DATAHOG_SETPOINT
static s16 getsp(void)
{
    s32 sp;
    s16 ret;
    ctlmode_t ctlmode = mode_GetEffectiveControlMode(&sp);
    if((ctlmode == CONTROL_OFF) || (sp == SETPOINT_INVALID))
    {
        ret = INT16_MIN; //indication of invalid value
    }
    else
    {
        ret = (s16)sp;
    }
    return ret;
}
#endif //DATAHOG_SETPOINT

#ifdef DATAHOG_PILOTPRESSURE
static s16 getpilotpres(void)
{
    return pres_GetPressureData()->Pressures[PRESSURE_PILOT_INDEX];
}
#endif

#ifdef DATAHOG_SUPPLYPRESSURE
static s16 getsupplypres(void)
{
    return pres_GetPressureData()->Pressures[PRESSURE_SUPPLY_INDEX];
}
#endif

#ifdef DATAHOG_PRESSUREB
static s16 getpresb(void)
{
    return pres_GetPressureData()->Pressures[PRESSURE_ACT2_INDEX];
}
#endif

#ifdef DATAHOG_IPCURRENT
static s16 getipcirr(void)
{
    return (s16)ipcurr_GetCompensatedIpCurrent();
}
#endif

//-------------------- end shims ---------------------------

static  DatahogFunc_t *const DatahogTable[] =
{
#ifdef DATAHOG_POSITION
    [DATAHOG_POSITION] = vpos_GetScaledPosition,
#endif
#ifdef DATAHOG_MAINPRESSURE
    [DATAHOG_MAINPRESSURE] = pres_GetMainPressure,
#endif
#ifdef DATAHOG_PILOTPRESSURE
    [DATAHOG_PILOTPRESSURE] = getpilotpres,
#endif
#ifdef DATAHOG_SUPPLYPRESSURE
    [DATAHOG_SUPPLYPRESSURE] = getsupplypres,
#endif
#ifdef DATAHOG_PRESSUREB
    [DATAHOG_PRESSUREB] = getpresb,
#endif
#ifdef DATAHOG_SETPOINT
    [DATAHOG_SETPOINT] = getsp,
#endif
#ifdef DATAHOG_IPCURRENT
    [DATAHOG_IPCURRENT] = getipcirr,
#endif
};

static DatahogState_t DatahogState;

static const DatahogState_t DatahogState_default =
{
    .datamask = 0U,
    .status = DatahogIdle,
    .skip_count = 0,
    .skipsleft = 0,
    .CheckWord = 0U, //don't care
};

/** \brief Initialization function
*/
void datahog_Init(void)
{
    ErrorCode_t err = datahog_Set(NULL);
    MN_ASSERT(err == ERR_OK);
}

/** \brief Get function
*/
const DatahogState_t *datahog_Get(DatahogState_t *dst)
{
    return STRUCT_TESTGET(&DatahogState, dst);
}

/** \brief Set function
*/
ErrorCode_t datahog_Set(const DatahogState_t *src)
{
    if(src == NULL)
    {
        src = &DatahogState_default;
    }
    //Check that all data requests have valid collection functions
    ErrorCode_t err = ERR_OK;
    u16_least mask = 1U;
    for(size_t i=0; i<sizeof(src->datamask)*CHAR_BIT; i++)
    {
        if((mask & src->datamask) != 0U)
        {
            //Collection requested
            if((i >= NELEM(DatahogTable)) || (DatahogTable[i] == NULL))
            {
                err = ERR_NOT_SUPPORTED;
                break;
            }
        }
        mask <<= 1;
    }
    if(err == ERR_OK)
    {
        Struct_Copy(DatahogState_t, &DatahogState, src);
    }
    return err;
}

#define SafeStoreInt(structptr, member, value) \
    MN_ENTER_CRITICAL(); \
    storeMemberInt((structptr), member, (value)); \
    MN_EXIT_CRITICAL()

#define DIAG_DATAHOG ((diag_t)0x5500)
#define DIAG_DATAHOG_VERSION 0

#define BUFFERPLACE_HEADERSIZE 2
#define BUFFERPLACE_NUMSAMPLES 3


static void FillDiagHeader(u16 sample_bitmap, u16 skip_count)
{
    //Populate the buffer header
    const diag_t ExtDiag_Header[] =
    {
        [0] = DIAG_DATAHOG, //test type
        [1] = DIAG_DATAHOG_VERSION, //version
        [BUFFERPLACE_HEADERSIZE] = EXTDIAG_HEADERSZ, //header size in diag_t entries
        [BUFFERPLACE_NUMSAMPLES] = 0, // #of samples collected
        [4] = (diag_t)sample_bitmap,
        [5] = (diag_t)MIN(INT16_MAX,
                      (skip_count+1) * CYCLE_TASK_DIVIDER * CTRL_TASK_DIVIDER), //Sampling interval in 5 ms ticks
    };

    buffer_InitializeXDiagnosticBuffer(DIAGBUF_DEFAULT);

    DIAGRW_WriteBufferHEADER(ExtDiag_Header);
    u16 buf_init_entries = EXTDIAG_HEADERSZ;
    buffer_SelectRange(DIAGBUF_DEFAULT, NULL, &buf_init_entries); //OK, had to to initialize

}


/** \brief Simple high-speed collector of various data per bitmask
*/
void datahog_Collect(void)
{
    Struct_Test(DatahogState_t, &DatahogState);
    switch(DatahogState.status)
    {
        case DatahogCollecting:
        {
            if(process_GetProcId() != PROC_NONE)
            {
                SafeStoreInt(&DatahogState, status, (u8)DatahogInterrupted);
            }
            else
            {
                u16_least mask = 1U;
                u16 skipsleft = DatahogState.skipsleft;
                if(skipsleft != 0U)
                {
                    --skipsleft;
                    SafeStoreInt(&DatahogState, skipsleft, skipsleft);
                }
                else
                {
                    //Restart counting
                    SafeStoreInt(&DatahogState, skipsleft, DatahogState.skip_count);
                    bool_t bovfl = false; //indicator of buffer overflow
                    //And collect data
                    for(size_t i=0; i<MIN(NELEM(DatahogTable), sizeof(DatahogState.datamask)*CHAR_BIT); i++)
                    {
                        if((mask & DatahogState.datamask) != 0U)
                        {
                            //Collection requested
                            s16 val = DatahogTable[i]();
                            bovfl = buffer_AddXDiagnosticData1(DIAGBUF_DEFAULT, val);
                            if(bovfl) //buffer full?
                            {
                                SafeStoreInt(&DatahogState, status, (u8)DatahogCompleted);
                                break;
                            }
                        }
                        mask <<= 1;
                    }
                    if(!bovfl)
                    {
                        //Increment counter in the header
                        buffer_GetXDiagnosticBuffer(DIAGBUF_DEFAULT)[BUFFERPLACE_NUMSAMPLES]++;
                    }
                }
            }
            break;
        }
        case DatahogCompleted:
        case DatahogStop:
        {
            if(process_GetProcId() != PROC_NONE)
            {
                SafeStoreInt(&DatahogState, status, (u8)DatahogInterrupted);
            }
            break;
        }
        case DatahogStart:
        {
            //Create a header here
            if(process_GetProcId() != PROC_NONE)
            {
                SafeStoreInt(&DatahogState, status, (u8)DatahogInterrupted);
            }
            else
            {
                FillDiagHeader(DatahogState.datamask, DatahogState.skip_count);
                SafeStoreInt(&DatahogState, status, (u8)DatahogCollecting);
            }
            break;
        }
        case DatahogIdle:
        default:
        {
            break; //nothing to do
        }
    }
}




