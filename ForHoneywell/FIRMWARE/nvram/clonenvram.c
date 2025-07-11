/*
Copyright 2013 by Dresser, Inc., as an unpublished trade secret.  All rights reserved.
This document and all information herein are the property of Dresser, Inc.
It and its contents are  confidential and have been provided as a part of
a confidential relationship.  As such, no part of it may be made public,
decompiled, reverse engineered or copied and it is subject to return upon
demand.
*/
/**
    \file clonenvram.c
    \brief Copy NVMEM to the opposite bank

    CPU: Any

    OWNER: AK

    \ingroup nvram
*/
#define NEED_NVMEM_END
#include "mnwrap.h"
#include "process.h"
#include "fram.h"
#include "faultpublic.h"

#include "nvmempriv.h"
#include "mnassert.h"
//#include MEMMAP_H_
#include "clonenvram.h"
#include "diagrw.h"

#include "bufferhandler.h"
#include "vertranpub.h"
#include "stm_vectors.h"
#include "mn_flash.h"
#include "evertran.h"
#include "crc.h"
#include "bsp.h"
#include "numutils.h"
#include "hmac.h"

#include "timebase.h"

#include "hart.h" //for device id

#define EXDATA_SIGNATURE (0xAA55U)
//lint -esym(768, ExtraData_t::*) OK if not referenced (global)
//lint -esym(754, ExtraData_t::*) OK if not referenced (local)
typedef struct ExtraData_t
{
    u16 Signature;
    u16 version;
    const u16 *crcPtr;
    const u16 *dummyPtr;
    const VerInfo_t *VerInfo;
    ErrorCode_t (*vertran)(void * const *src, void **dst, u16_least srcVer, u16_least dstVer, void (*func)(void));
    const struct xlatef_t *xTable; //!< NVMEM translation table
} ExtraData_t;


//Redundant-ish definitions for the first cut
//lint -emacro({826}, CHECKWORD_ADDR1)  area too small:  The checksum is the two bytes beyond (dst + len)
#define CHECKWORD_ADDR1(addr, len) ((u16 *)((u8*)(addr) + (len)))

#define HALFWORD_HIGH1(word) (((u32)word)>>16) //!< extractor of low 16 bits of a word
#define NVRAM_CRC_SIZE1 sizeof(u16) //!< the number of bytes in NV memory that a CRC entry takes
#define length2halfwords1(len) ((len)/2U) //!< (even) length in bytes (8 bits) converted to length in halfwords (16 bits)

#define NV_LAYOUT_LEGACY 8U //! default layout if none other is specified

/** \brief Non-interruptible wait routine
*/
static void stdwait(void)
{
    (void)process_WaitForTimeExt(1U, CANCELPROC_MODE); //let periodic services (mopup and WD) run
}

/** \brief Non-interruptible wait routine
*/
static void shortwait(void)
{
    process_SetWatchdogFlag(WD_PROCESS_TASK_RUN);
}

/** \brief Process of loading non-volatile data in the buffer for further use
Clears the (more or less) standard buffer header for retriever's parser
\return a pointer to the (diagnostic) buffer
*/
static void *nvmem_LoadImage(void)
{
    void *src = buffer_GetXDiagnosticBuffer(DIAGBUF_DEFAULT) + NVXLATE_HEADERSZ;

    mn_memset(src, 0, NVXLATE_HEADERSZ*sizeof(diag_t));

    u8 *bufsrc = src;

    nvramId_fast_t id;
    for(id=0; id<NVRAM_ENTRIES; id++)
    {
        u8 *data = bufsrc + nvram_map[id].offset;

        const void *result = nvmem_GetItemById(data, id);
        MN_RT_ASSERT(result != NULL); //it is a catastrophic failure not to be able to retrieve data

        CONST_ASSERT((NV_HEADERSIZE % 4U) == 0);
        CONST_ASSERT((NV_NUMCOPIES % 2U) == 0);
        /* Alignment consideration: We just got lucky here:
        We need a 4-byte alignment, and we currently get it for free because
        diagnostic buffer is aligned, and each object's image copy gets aligned
        because we have 2 copies of 2-byte-aligned (because of CheckWord)
        structures. Had it not been so, we'd need a combersome data movement
        to ensure alignment
        */

        //We can read 8 objects without the watchdog kicking, can't we?
        if( (id % 8U) == 0)
        {
            shortwait();
        }
    }

    stdwait();
    return src;
}

/** \brief Finds "Extra Data" in the inactive flash image
\return a pointer to extra data (or NULL if can't find it)
*/
static const ExtraData_t *flash_FindExtraData(void)
{
    // get base address of non-running bank
    u32 flashbase = flash_GetPartitionBaseAddress(0);
    const vectors_t *vectors = HARDWARE(const vectors_t *, flashbase);
    const ExtraData_t *exd = vectors->ExtraData;
    /* exd must be valid by range, or we say it is a legacy build or nothing
    sensible. In either case, translating to legacy layout looks harmless
    */
#if 0
    if( ((const void *)exd > (const void *)vectors)
       && ((const void *)exd < (const void *)vectors->crcPtr)
       && (exd->Signature == EXDATA_SIGNATURE)
           )
#else
    if( ((uintptr_t)exd > (uintptr_t)vectors) && ((uintptr_t)exd < (uintptr_t)vectors->crcPtr) //lint !e923 the purpose of uintptr_t!
       && (exd->Signature == EXDATA_SIGNATURE)
           )
#endif
    {
        //OK
    }
    else
    {
        exd = NULL;
    }
    return exd;
}

/** \brief Discovers the target NVMEM layout number wanted by the inactive flash image

NOTE: Invalid Extra Data assumes layout 8 which is in first release
\param exd - a pointer to extra data in the inactive flash image
\return the discovered required layout number
*/
static u8 nvmem_FindTargetLayoutNumber(const ExtraData_t *exd)
{
    u8 target_layout;
    //Depends on what the other flash bank has:
    if(exd == NULL)
    {
        //No valid extra data; assume layout 8 (legacy)
        target_layout = NV_LAYOUT_LEGACY;
    }
    else
    {
        const VerInfo_t *VerInfo = exd->VerInfo;
        target_layout = VerInfo->nvram_version[0];
    }
    return target_layout;
}

/** \brief Discovers the NVMEM address of the block wanted by inactive flash image.
NOTE: block for layout 8 (legacy) is always at offset 0
\return the address offset
*/
MN_INLINE u16_least nvmem_FindDstOffset(u8 target_layout, bool_t thisbank)
{
    u8 nv_bank;
#if (NVRW_OPTIONS & NVRWOPT_USE_LEGACY_BANK) != 0
    if(target_layout <= NV_LAYOUT_LEGACY)
    {
        //A wart for legacy builds
        nv_bank = 2;
    }
    else
#else
     UNUSED_OK(target_layout);
#endif
    {
        //normal case
        nv_bank = flash_GetRunningPartitionID();
        if(!thisbank)
        {
            nv_bank = 1U - nv_bank;
        }
    }
    return nvmem_banks[nv_bank].offset; //start of FRAM block
}

/** \brief Process of converting the checksums to CRCs in the RAM image of NVMEM
NOTE: Close enough to but different enough from "Commit NVMEM operation. Perhaps,
at some point they can be merged together.
\param pxTable - a pointer to the controlling row of translation table
\param buffer - a pointer to the RAM image to write
\return an error code
*/
static ErrorCode_t nvmem_ConvertImageToCrc(const nvramDumpMap_t *pmap, nvramId_fast_t num_entries, void *buffer)
{
    nvramId_fast_t id = num_entries;
    ErrorCode_t err = ERR_OK;

    //Convert checksums to CRC
    do
    {
        --id;
        u16_least len = pmap[id].length;
        u8 *object = buffer;
        object += pmap[id].offset;

        //replace checksums with CRCs
        u16 *pCheckWord = CHECKWORD_ADDR1(object, len - NVRAM_CRC_SIZE1);
        u32 crcchecksum = hCrcChsum16(CRC_SEED, object, length2halfwords1(len - NVRAM_CRC_SIZE1));
        if(*pCheckWord == (u16)(crcchecksum))
        {
            *pCheckWord = HALFWORD_HIGH1(crcchecksum);
            stdwait();
        }
        else
        {
            err = ERR_NVMEM_CRC;
            break;
        }
    } while(id != 0);

    return err;
}

/** \brief Process of committing the RAM image of NVMEM to the specified place in NVMEM
NOTE: Close enough to but different enough from "Commit NVMEM operation. Perhaps,
at some point they can be merged together.
\param pxTable - a pointer to the controlling row of translation table
\param buffer - a pointer to the RAM image to write
\param dst_offset - starting address of the target image in NVMEM
\return an error code
*/
static ErrorCode_t nvmem_WriteImage(const nvramDumpMap_t *pmap, nvramId_fast_t num_entries, void *buffer, u16_least dst_offset)
{
    nvramId_fast_t id = num_entries;
    ErrorCode_t err = ERR_OK;
    //Write in reverse order so that NVMEM signature is written last
    do
    {
        --id;
        u16_least len = pmap[id].length;
        u32 nvaddr = pmap[id].offset + dst_offset;
        u8 *object = buffer;
        object += pmap[id].offset;
        MN_FRAM_ACQUIRE();
            for(u8_least i=0; i<NV_NUMCOPIES; i++)
            {
                err = fram_WriteExt(object, nvaddr, len, 0); //Only volume 0 is now supported
                if(err != ERR_OK)
                {
                    break;
                }
                nvaddr += len;
            }
        MN_FRAM_RELEASE();

        if(err == ERR_OK)
        {
            stdwait();
        }
        else
        {
            break;
        }
    } while(id != 0);
    return err;
}

static s16 nvmem_VerifyImage(u16_least dst_offset, u8 target_layout)
{
    s16 layout_stored;
    StartupVersionNumber_t sobject;
    ErrorCode_t err;
    MN_FRAM_ACQUIRE();
        err = fram_ReadExt((void *)&sobject, dst_offset, sizeof(sobject), 0); //Only volume 0 is now supported
    MN_FRAM_RELEASE();
    if((err != ERR_OK)
       || (sobject.CheckWord != hCrc16(CRC_SEED, &sobject, MN_OFFSETOF(StartupVersionNumber_t, CheckWord)/sizeof(u16)))
       || (((sobject.VersionNumber & 0xFFFF0000U) | target_layout) != sobject.VersionNumber)
           )
    {
        layout_stored = INT16_MAX; //no valid image at activation
    }
    else
    {
        layout_stored = (diag_t)(sobject.VersionNumber & 0x0000FFFFU);
    }
    return layout_stored;
}

/** \brief A simple wrapper to compute the NVMEM image length
\param pxTable - a pointer to the row in the translation table corresponding to the layout
\return the length of the image
*/
static u16_least nvmem_CalcTotalLength(const xlatef_t *pxTable)
{
    return pxTable->map[pxTable->num_entries-1].offset
        + (pxTable->map[pxTable->num_entries-1].length * NV_NUMCOPIES + DIAGRW_HEADERSZ);
}

/** \brief Process of cloning nvram to the opposite bank with layout translation
(opposite flash download support)
Actually, we grab source data from RAM; it is faster and more up-to-date, and,
besides, we don't need the mess with 2nd copy.
\return a process completion code
*/
procresult_t nvmem_Clone(s16 *procdetails)
{
    UNUSED_OK(procdetails);

    void *src = nvmem_LoadImage(); //Address in the (diagnostic) buffer
    u8 *bufsrc = src;

    const ExtraData_t *exd = flash_FindExtraData();
    u8 target_layout = nvmem_FindTargetLayoutNumber(exd);
    u8 source_layout = VerString[0].nvram_version[0];

    ErrorCode_t err = ERR_OK;
#if (NVRW_OPTIONS & NVRWOPT_USE_LEGACY_BANK) == 0
    if(exd == NULL)
    {
        /* We assume it's Release1 in the opposite bank
        We can only deal with it if
        1. source_layout == NV_LAYOUT_LEGACY, in which case no translation needed, or
        2. we are running from flash/nvmem bank 1, so translated image goes to
           bank 0 where Release1 expects it.
        If neither condition is met, we shall fail to activate Release1
        */
        if(source_layout == NV_LAYOUT_LEGACY)
        {
            //OK
        }
        else if(flash_GetRunningPartitionID() != 0) //i.e. == 1
        {
            //OK
        }
        else
        {
            err = ERR_NOT_SUPPORTED;
        }
    }
#endif //(NVRW_OPTIONS & NVRWOPT_USE_LEGACY_BANK) == 0

    //Now let's do the translation
    ErrorCode_t (*pvertran)(void * const *src, void **dst, u16_least srcVer, u16_least dstVer, void (*func)(void));
    const xlatef_t *pxTable;

#ifdef _lint
    if((target_layout == source_layout) || (target_layout < source_layout))
    /* What was meant above is of course
        if(target_layout <= source_layout)
       Mysteriously, lint finds potential off-by-one on target_layout there
    */
#else
    if(target_layout <= source_layout)
#endif
    {
        //we know how to do this
        pvertran = vertran;
        pxTable = xTable;
        CONST_ASSERT(NELEM(xTable) > NV_LAYOUT_LEGACY);
    }
    else
    {
        MN_DBG_ASSERT(exd != NULL);
        //the other build has to do the translation
        pvertran = exd->vertran;
        pxTable = exd->xTable;
    }

    /* NOTE: Both pxTable[target_layout] and pxTable[source_layout] are valid
    by construction. Indeed,
    If target_layout <= source_layout, pxTable is of THIS flash image and is
    aware of target_layout
    Otherwise, pxTable is of the OTHER flash image, and that image is valid (or
    else target_layout could not be > source_layout), and that other flash image
    is aware of THIS image's layout
    */
    void *vdst = bufsrc + NVMEM_STD_BANK_SIZE + NVMEM_BANK_GAP;


    if(err == ERR_OK)
    {
        err = pvertran(&src, &vdst, source_layout, target_layout, shortwait);
    }
    u8 *dst = vdst;
    u16_least dst_offset = nvmem_FindDstOffset(target_layout, false); //start of FRAM block

    procresult_t procresult;

    if(err == ERR_OK)
    {
        err = nvmem_ConvertImageToCrc(pxTable[target_layout].map, pxTable[target_layout].num_entries,  dst);
    }
    //And now, we can write the translated image to the opposite bank.
    if(err == ERR_OK)
    {
        err = nvmem_WriteImage(pxTable[target_layout].map, pxTable[target_layout].num_entries,  dst, dst_offset);

        if(err == ERR_OK)
        {
            procresult = PROCRESULT_OK; //vertran and write out OK
        }
        else
        {
            procresult = PROCRESULT_FAILED; //vertran OK but write out failed
        }
    }
    else
    {
        procresult = PROCRESULT_CANCELED; //vertran failed
    }

    //Now check to see what is in the other bank now
    s16 layout_stored = nvmem_VerifyImage(dst_offset, target_layout);
    if(layout_stored == INT16_MAX) //error
    {
        procresult = PROCRESULT_FAILED; //no valid image at activation
    }

    u16_least target_nvtotal = nvmem_CalcTotalLength(&pxTable[target_layout]);
    u16_least source_nvtotal = nvmem_CalcTotalLength(&pxTable[source_layout]);

    //Populate the buffer header
    const diag_t nvxlate_Header[] =
    {
        [nvindex_buftype] = NVXLATE_DIAGTYPE_XLATE, //"test" type
        [nvindex_bufversion] = NVXLATE_VERSION, //version
        [nvindex_headersize] = NVXLATE_HEADERSZ, //header size in diag_t entries
        [nvindex_sizein] = (diag_t)(source_nvtotal/sizeof(u16)), // #of samples
        [nvindex_sizeout] = (diag_t)(target_nvtotal/sizeof(u16)), // #of samples
        [nvindex_versionin] = (diag_t)source_layout,
        [nvindex_versionout] = (diag_t)target_layout,
        [nvindex_version_stored] = (diag_t)layout_stored,
        [nvindex_result1] = (diag_t)procresult,
        [nvindex_result2] = (diag_t)procresult, // repeat
        [nvindex_initoffset] = (diag_t)(dst - bufsrc), //lint !e946 !e947 pointer subtraction - they point to the same buffer array so OK
    };
    DIAGRW_WriteBufferHEADER(nvxlate_Header);
    u16 len16 = NVXLATE_HEADERSZ;
    buffer_SelectRange(DIAGBUF_DEFAULT, NULL, &len16); //don't divulge much data; only header


    return procresult;
}

#define NV_CHUNK 64 //more or less arbitrary read/write chunk

/** \brief Copies inactive NV memory betweein RAM and FRAM with waits inserted
NOTE: Doesn't check for "inactive"
\param ram - a pointer to RAM buffer
\param offset - offset (address) in FRAM
\param len - length of data in bytes
\param dir - direction (true = Read from FRAM to RAM; false = Write RAM to FRAM)
*/
static diag_t nvmem_CopyInactiveNV(u8 *ram, size_t offset, size_t len, bool_t dir)
{
    diag_t ret = NVRW_COMPLETION_OK;
    size_t orglen = len;
    while(len > 0)
    {
        size_t chunk = MIN(len, NV_CHUNK);
        ErrorCode_t err;
        MN_FRAM_ACQUIRE();
            if(dir)
            {
                err = fram_ReadExt(ram, offset, len, 0);
            }
            else
            {
                err = fram_WriteExt(ram, offset, len, 0);
            }
        MN_FRAM_RELEASE();
        process_SetProcessProgress((u8) ( ((orglen-len)*100 )/orglen) ); //rough ballpark idea
        (void)process_WaitForTime(MN_MS2TICKS(50));
        len -= chunk;
        offset += chunk;
        ram += chunk;
        if(err != ERR_OK)
        {
            ret = NVRW_RWERROR;
            break;
        }
    }
    return ret;
}


/** \brief Process of preparing NVMEM image corresponding to the running firmware
\return PROCRESULT_OK (can't fail)
*/
procresult_t nvmem_PrepareUpload(s16 *procdetails)
{
    UNUSED_OK(procdetails);
    diag_t *dbuf = buffer_GetXDiagnosticBuffer(DIAGBUF_DEFAULT);
    diag_t nvbank = dbuf[nvindex_thisbank]; //The fishy interface with the HART command
    diag_t completion_code;
    diag_t source_layout = -1; //must be discovered from image by default
    u8 *start = (u8 *)(dbuf + DIAGRW_HEADERSZ);

    u16_least source_nvtotal = 0; //empty by default
    switch(nvbank)
    {
        case NVBANK_ACTIVE:
        {
            start = nvmem_LoadImage(); //skip the address in the (diagnostic) buffer
            completion_code = NVRW_COMPLETION_OK;
            source_layout = VerString[0].nvram_version[0];
            source_nvtotal = nvmem_CalcTotalLength(&xTable[source_layout]);
            break;
        }
        case NVBANK_INACTIVE:
        {
            if(0 == flash_GetRunningPartitionID())
            {
                nvbank = NVBANK_1;
            }
            else
            {
                nvbank = NVBANK_0;
            }
        }
        //lint -fallthrough

        case NVBANK_0:
        case NVBANK_1:
#if (NVRW_OPTIONS & NVRWOPT_USE_LEGACY_BANK) != 0
        case NVBANK_2:
#endif
        {
            if((nvbank - NVBANK_0) == flash_GetRunningPartitionID())
            {
                completion_code = NVRW_PROHIBITED;
            }
            else
            {
                source_nvtotal = nvmem_banks[nvbank - NVBANK_0].length;
                completion_code = nvmem_CopyInactiveNV(start,
                                        nvmem_banks[nvbank - NVBANK_0].offset, source_nvtotal, true);
            }
            break;
        }
        default:
        {
            completion_code = NVRW_NOT_SUPPORTED;
            break;
        }

    }

    const ExtraData_t *exd = flash_FindExtraData();
    u8 target_layout = nvmem_FindTargetLayoutNumber(exd);

    size_t len = source_nvtotal
#if 0 //we don't include layout table yet
        + pxTable->num_entries*sizeof(nvramDumpMap_t)
#endif
        + DIAGRW_HEADERSZ*sizeof(diag_t);
    diag_t len16 = (diag_t)((len + SHA256_BLOCK_SIZE)/sizeof(diag_t));
    const u8 *devid = hart_GetHartData()->device_id;
    diag_t devid16[] = {0, 0, 0};
    CONST_ASSERT(sizeof(devid16) >= HART_ID_LENGTH);
    MN_ENTER_CRITICAL();
        mn_memcpy(devid16, devid, HART_ID_LENGTH);
    MN_EXIT_CRITICAL();

    //Populate the buffer header
    const diag_t nvread_Header[] =
    {
        [nvindex_buftype] = NVXLATE_DIAGTYPE_READ, //"test" type
        [nvindex_bufversion] = NVXLATE_VERSION, //version
        [nvindex_headersize] = DIAGRW_HEADERSZ, //header size in diag_t entries
        [nvindex_sizein] = len16, // #of samples
        [nvindex_sizeout] = len16, // #of samples
        [nvindex_versionin] = (diag_t)source_layout,
        [nvindex_versionout] = (diag_t)target_layout, // for read, reserved
        [nvindex_result1] = completion_code,
        [nvindex_result2] = completion_code, // repeat
        [nvindex_version_stored] = (diag_t)0, // for read, reserved
        [nvindex_initoffset] = DIAGRW_HEADERSZ,
        [nvindex_thisbank] = nvbank,
        [nvindex_deviceid1] = devid16[0],
        [nvindex_deviceid2] = devid16[1],
        [nvindex_deviceid3] = devid16[2],
    };
    DIAGRW_WriteBufferHEADER(nvread_Header);
    buffer_SelectRange(DIAGBUF_DEFAULT, NULL, (u16 *)&len16);

    //Append signature to the image, starting at `start'; keep the header in the signature
    if(completion_code == NVRW_COMPLETION_OK)
    {
        start += source_nvtotal;
        hmac_init(stdwait);
        hmac_update(buffer_GetXDiagnosticBuffer(DIAGBUF_DEFAULT), len, shortwait);
        hmac_final(start, stdwait);
    }

    return PROCRESULT_OK;
}

static diag_t nvmem_TestBufferActiveImage(diag_t *src)
{
    diag_t ret = NVRW_COMPLETION_OK;
    u8 source_layout = VerString[0].nvram_version[0];
    u16_least source_nvtotal = nvmem_CalcTotalLength(&xTable[source_layout]);

    nvramId_fast_t id = NVRAM_ENTRIES;

    //Do we have enough data to write?  verify that the table size is correct;
    if(((u16)src[nvindex_sizein])*sizeof(diag_t) < source_nvtotal)
    {
        ret = NVRW_DATA;
    }
    else
    {

        u16 *buffer = (u16 *)(src + DIAGRW_HEADERSZ);

        //Verify all CRCs
        do
        {
            --id;
            u16_least len = nvram_map[id].length;
            u8 *object = ((u8 *)buffer) + nvram_map[id].offset;

            //Check CRC
            const u16 *pCheckWord = CHECKWORD_ADDR1(object, len);
            u32 crcchecksum = hCrcChsum16(CRC_SEED, object, length2halfwords1(len));
            if(*pCheckWord != HALFWORD_HIGH1(crcchecksum))
            {
                ret = NVRW_DATA;
                break;
            }

            shortwait();
        } while(id != 0);
    }

    return ret;
}

/** \brief Helper process to write out a downloaded NVMEM image
\return a process completion code
*/
procresult_t nvmem_WriteImageProc(s16 *procdetails)
{
    diag_t ret = NVRW_COMPLETION_OK;
    UNUSED_OK(procdetails);
    u8 source_layout = 0; //for any inactive bank
    //First, verify the header
    diag_t *src = buffer_GetXDiagnosticBuffer(DIAGBUF_DEFAULT);
    u16 *usrc = (u16 *)src;

    size_t len = usrc[nvindex_sizein]; //length in 16-bits

    if(
       /* We decided against it so that the signature doesn't need to be recomputed
       (src[nvindex_buftype] != NVXLATE_DIAGTYPE_WRITE)
       ||
       */
          (src[nvindex_bufversion] != NVXLATE_VERSION)
       || (src[nvindex_headersize] != NVXLATE_HEADERSZ)
       || (src[nvindex_initoffset] != NVXLATE_HEADERSZ)
       || (len < SHA256_BLOCK_SIZE) //pretty rough but OK for now
       || (usrc[1+NVXLATE_HEADERSZ] != MNCB_MAGIC_NUMBER)
           )
    {
        ret = NVRW_DATA;
    }

    if(ret == NVRW_COMPLETION_OK)
    {
        const u8 *devid = hart_GetHartData()->device_id;
        MN_ENTER_CRITICAL();
            if(mn_memcmp(devid, &src[nvindex_deviceid1], HART_ID_LENGTH) != 0)
            {
                if(!error_IsFault(FAULT_FACTORYMODE))
                {
                    ret = NVRW_WRONG_DEVICE;
                }
            }
        MN_EXIT_CRITICAL();
    }

    if(ret == NVRW_COMPLETION_OK)
    {
        //verify the checksum/signature
        diag_t *end = src + len; //just past the image's signature
        len -= SHA256_BLOCK_SIZE/sizeof(diag_t);

        hmac_init(stdwait);
        hmac_update(src, len*sizeof(diag_t), shortwait);
        hmac_final((u8 *)end, stdwait);

        diag_t *sign = end - SHA256_BLOCK_SIZE/sizeof(diag_t);

        if(mn_memcmp(sign, end, SHA256_BLOCK_SIZE) != 0)
        {
            ret = NVRW_SIGNATURE;
        }
    }

    diag_t nvbank = src[NVCLONE_MAX_IMAGE_LENGTH]; //where the HART command stuffed it
    src[nvindex_thisbank] = nvbank; //put it in response header
    if(ret == NVRW_COMPLETION_OK)
    {
        switch(nvbank)
        {
            case NVBANK_ACTIVE:
            {
                source_layout = VerString[0].nvram_version[0];
                if(src[nvindex_versionin] != NVRAM_VERSION)
                {
                    ret = NVRW_DATA;
                }
                else
                {
                    ret = nvmem_TestBufferActiveImage(src); //skip the address in the (diagnostic) buffer
                }
                if(ret == NVRW_COMPLETION_OK)
                {
                    const nvramDumpMap_t *pmap = xTable[source_layout].map;
                    MN_ASSERT(pmap != NULL);
                    u16 dst_offset = fram_GetRemapOffset(0);
                    ErrorCode_t err = nvmem_WriteImage(pmap, NVRAM_ENTRIES, src + NVXLATE_HEADERSZ, dst_offset);
                    if(err != ERR_OK)
                    {
                        ret = NVRW_RWERROR;
                    }
                }

                break;
            }

            case NVBANK_INACTIVE:
            {
                if(0 == flash_GetRunningPartitionID())
                {
                    nvbank = NVBANK_1;
                }
                else
                {
                    nvbank = NVBANK_0;
                }
            }
            //lint -fallthrough

            case NVBANK_0:
            case NVBANK_1:
    #if (NVRW_OPTIONS & NVRWOPT_USE_LEGACY_BANK) != 0
            case NVBANK_2:
    #endif
            {
                if((nvbank - NVBANK_0) == flash_GetRunningPartitionID())
                {
                    ret = NVRW_PROHIBITED;
                }
                else
                {
                    u16_least source_nvtotal = nvmem_banks[nvbank - NVBANK_0].length;
                    ret = nvmem_CopyInactiveNV((u8 *)(src + NVXLATE_HEADERSZ),
                                            nvmem_banks[nvbank - NVBANK_0].offset, source_nvtotal, false);
                }
                break;
            }
            default:
            {
                ret = NVRW_NOT_SUPPORTED;
                break;
            }
        }
    }


#if 0 //in the future, maybe
    nvramId_fast_t num_entries = (nvramId_fast_t)usrc[nvindex_numentries];
    //by convention the layout table is appended
    const nvramDumpMap_t *pmap = (const void *)(src + src[nvindex_sizein]);
    //Now verify that the table size is correct;
    if(usrc[nvindex_sizeout] != ((num_entries * sizeof(nvramDumpMap_t))
                                + usrc[nvindex_sizein]))
    {
        procresult = PROCRESULT_FAILED;
    }
    u16 *buffer = (usrc + NVXLATE_HEADERSZ);
#endif

    diag_t len16 = (diag_t)(len + SHA256_BLOCK_SIZE/sizeof(diag_t));
    //Populate the buffer header
    const diag_t nvwrite_Header[] =
    {
        [nvindex_buftype] = NVXLATE_DIAGTYPE_WRITE, //"test" type
        [nvindex_bufversion] = NVXLATE_VERSION, //version
        [nvindex_headersize] = DIAGRW_HEADERSZ, //header size in diag_t entries
        [nvindex_sizein] = len16, // #of samples
        [nvindex_sizeout] = len16, // #of samples
        [nvindex_versionin] = (diag_t)source_layout,
        [nvindex_versionout] = (diag_t)source_layout, // for read, reserved
        [nvindex_result1] = ret,
        [nvindex_result2] = ret, // repeat
        [nvindex_version_stored] = (diag_t)0, // for read, reserved
        [nvindex_initoffset] = DIAGRW_HEADERSZ,
        [nvindex_thisbank] = nvbank,
    };
    DIAGRW_WriteBufferHEADER(nvwrite_Header);
    buffer_SelectRange(DIAGBUF_DEFAULT, NULL, (u16 *)&len16);

    procresult_t procresult = PROCRESULT_OK;
    if(ret != NVRW_COMPLETION_OK)
    {
        procresult = PROCRESULT_FAILED;
    }

    return procresult;
}

/** \brief A dummy never-ending process just to indicate a running process
and doing standard NVRAM self-test under the hood.
*/
procresult_t nvram_DummyProcess(s16 *procdetails)
{

    for(;;)
    {
        (void)nvram_PageTest(procdetails);
        if(process_WaitForTimeExt(MN_MS2TICKS(200), CANCELPROC_MODE | CANCELPROC_POWERMARGIN) != 0)
        {
            break;
        }
    }

    return PROCRESULT_OK;
}

//---------------------- helpers -----------------------------

/** \brief Get the remapping offset
  \param[in] AreaSize - FRAM area size to generate the re-map offset (unused)
  \return Offset to be used with re-map
*/
u16 fram_GetRemapOffset(u16 AreaSize)
{
    UNUSED_OK(AreaSize);
    u32 Offset = nvmem_FindDstOffset(VerString[0].nvram_version[0], true);
    MN_DBG_ASSERT(Offset == ((u16)Offset));
    return (u16)Offset;
}


const ExtraData_t APPExtraData =
{
    .Signature = EXDATA_SIGNATURE,
    .version = 0,
    .crcPtr = &mychecksum,
    .dummyPtr = NULL,
    .VerInfo = VerString,
    .vertran = vertran,
    .xTable = xTable,
};

/* This line marks the end of the source */
