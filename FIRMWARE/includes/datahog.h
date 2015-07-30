#ifndef DATAHOG_H_
#define DATAHOG_H_
#include "errcodes.h"

//Bits
#define DATAHOG_POSITION 0
#define DATAHOG_MAINPRESSURE 1
#define DATAHOG_PILOTPRESSURE 2
#define DATAHOG_SUPPLYPRESSURE 3
#define DATAHOG_PRESSUREB 4
#define DATAHOG_SETPOINT 5
#define DATAHOG_IPCURRENT 6

typedef enum DatahogStatus_t
{
    DatahogIdle,
    DatahogInterrupted,
    DatahogCollecting,
    DatahogCompleted,
    DatahogStop,
    DatahogStart
} DatahogStatus_t;

typedef struct DatahogState_t
{
    u16 datamask;
    DatahogStatus_t status;
    u16 skip_count;
    u16 skipsleft;
    u16 CheckWord;
} DatahogState_t;

extern const DatahogState_t *datahog_Get(DatahogState_t *dst);
extern ErrorCode_t datahog_Set(const DatahogState_t *src);
extern void datahog_Collect(void);
extern void datahog_Init(void);

#endif //DATAHOG_H_
