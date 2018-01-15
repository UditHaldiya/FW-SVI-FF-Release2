#ifndef _MN_ADVANCED_H
#define _MN_ADVANCED_H
#include <ptb.h>
#include "mnhart2ff.h"

extern fferr_t ffcheck_WriteAdvanced(T_FBIF_BLOCK_INSTANCE * p_block_instance, T_FBIF_WRITE_DATA *p_write);
extern fferr_t ffcheck_WriteFilter(const T_FBIF_BLOCK_INSTANCE * p_block_instance, const T_FBIF_WRITE_DATA *p_write);
extern void ffcheck_ReadFilter
(
    const T_FBIF_BLOCK_INSTANCE * p_block_instance,
    const T_FBIF_READ_DATA *p_read
);
extern fferr_t ffcheck_AlertEnableChange(T_FBIF_BLOCK_INSTANCE * p_block_instance);
extern bool_t ffcheck_WriteBitmap(u8 index);
extern bool_t ffcheck_ReadBitmap(u8 index);
extern bool_t mn_GetBit(u8 bitNumber, const u32* Array, u8 length);

#define USIGNED_CHAR_NUMBER          256u

#endif //_MN_ADVANCED_H
