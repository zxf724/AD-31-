/*****************************************************************************
 *
 * 文件描述: SFlash记录管理
 * 输出参数: void
 * 修改记录:
 *
 *           1. 2015-1-20 chenyue Create
 *****************************************************************************/

/*****************************************************************************
**                          头文件包含和宏定义
******************************************************************************/
//#include "config.h"
#include "user_comm.h"

uint32 g_ulRecmEvenWIndex = 0;
uint32 g_ulRecmEvenRIndex = 0;

uint32 g_ulRecmElecWIndex = 0;
uint32 g_ulRecmElecRIndex = 0;


const RECM_INFO_S g_astSFlashInfo[] = {
    /* 终端事件记录 */
    {
        RECM_RECTYPE_EVEN,
        0,
        RECM_GETBLOCKUNITNUM(RECM_UNITLEN_EVEN),
        &g_ulRecmEvenWIndex,
        &g_ulRecmEvenRIndex,
        RECM_BLOCKSTART_EVEN,
        RECM_BLOCKTATOL_EVEN
    },
    /* 电量累计记录 */
    {
        RECM_RECTYPE_ELEC,
        0,
        RECM_GETBLOCKUNITNUM(RECM_UNITLEN_ELEC),
        &g_ulRecmElecWIndex,
        &g_ulRecmElecRIndex,
        RECM_BLOCKSTART_ELEC,
        RECM_BLOCKTATOL_ELEC
    },
};

#define RECM_INFO_MAXNUM  (sizeof(g_astSFlashInfo)/sizeof(RECM_INFO_S))

/*****************************************************************************
 * 函数名称: RECM_UpdateIndex
 * 函数功能: 修正索引号
 * 输入参数: void
 * 输出参数: void
 * 返回值  : uint32
 * 修改记录:
 *           1. 2015-3-17 zhengsk Create
 *****************************************************************************/
uint32 RECM_UpdateIndex(uint8 ucRecType, uint32 ulIndex)
{
    uint32 ulOldestIndex;
    uint32 ulLastIndex;

    ulOldestIndex = RECM_GetOldestIndex(ucRecType);
    ulLastIndex = RECM_GetLastIndex(ucRecType);

    if(ulOldestIndex > ulLastIndex)
    {
       if((ulIndex < ulOldestIndex) && (ulIndex > ulLastIndex))
       {
            ulIndex = ulLastIndex;
       }
    }
    else
    {
        if((ulIndex < ulOldestIndex) || (ulIndex > ulLastIndex))
        {
            ulIndex = ulLastIndex;
        }
    }

    return ulIndex;
}

/*****************************************************************************
 * 函数名称: RECM_GetInfoAddr
 * 函数功能: 根据索引号(类型)，获取参数
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2015-1-20 chenyue Create
 *****************************************************************************/
RECM_INFO_S *RECM_GetInfoAddr(uint8 ucRecType)
{
    uint32 i;

    for (i = 0; i < RECM_INFO_MAXNUM; i++)
    {
        if (ucRecType == g_astSFlashInfo[i].ucRecType)
        {
            return (RECM_INFO_S *)(&(g_astSFlashInfo[i]));
        }
    }

    return NULL;
}

/*****************************************************************************
 * 函数名称: RECM_GetAddr
 * 函数功能: 获取索引记录的起始地址
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2015-3-16 chenyue Create
 *****************************************************************************/
uint32 RECM_GetAddr(RECM_INFO_S *pstSFlashInfo, uint32 ulIndex)
{
    uint32 ulOffset;
    uint32 ulTmp;

    if (0 == pstSFlashInfo->ulBlockUnitNum)
    {
        /* 单条记录使用多块存储 */
        ulOffset = pstSFlashInfo->ulUnitLen / RECM_BLOCK_SIZE;
        if (0 != (pstSFlashInfo->ulUnitLen % RECM_BLOCK_SIZE))
        {
            ulOffset++;
        }
        ulTmp = ulOffset;
        ulOffset = pstSFlashInfo->ulTotalBlockNum / ulOffset;
        ulOffset = ulIndex % ulOffset;
        ulOffset *= ulTmp;
        ulOffset += pstSFlashInfo->ulStartBlockNum;

        ulOffset *= RECM_BLOCK_SIZE;
    }
    else
    {
        ulOffset = ulIndex / pstSFlashInfo->ulBlockUnitNum;
        ulOffset %= pstSFlashInfo->ulTotalBlockNum;
        ulOffset += pstSFlashInfo->ulStartBlockNum;

        ulOffset *= RECM_BLOCK_SIZE;
        ulOffset += (ulIndex % pstSFlashInfo->ulBlockUnitNum) * pstSFlashInfo->ulUnitLen;
    }

    return ulOffset;
}

#if 0
/*****************************************************************************
 * 函数名称: RECM_UpdateReadIndex
 * 函数功能: 更新存储的最老记录索引号
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2015-3-16 chenyue Create
 *****************************************************************************/
void RECM_UpdateReadIndex(RECM_INFO_S *pstSFlashInfo, uint32 ulWIndex, uint32 ulRIndex)
{
    uint32 ulTmp;

    if (0 == pstSFlashInfo->ulBlockUnitNum)
    {
        ulTmp = pstSFlashInfo->ulUnitLen / RECM_BLOCK_SIZE;
        if (0 != (pstSFlashInfo->ulUnitLen % RECM_BLOCK_SIZE))
        {
            ulTmp++;
        }
        ulTmp = pstSFlashInfo->ulTotalBlockNum / ulTmp;
        ulTmp -= 1;
    }
    else
    {
        if (0 == (ulWIndex % pstSFlashInfo->ulBlockUnitNum))
        {
            /* 要擦除一个块 */
            if (ulWIndex != ulRIndex)
            {
                /* 此句可优化为常量 */
                ulTmp = pstSFlashInfo->ulBlockUnitNum * (pstSFlashInfo->ulTotalBlockNum - 1);
            }
        }
        else
        {
            return;
        }
    }

    if (ulWIndex >= ulRIndex)
    {
        if (ulWIndex > ulTmp)
        {
            ulTmp = ulWIndex - ulTmp;
        }
        else
        {
            ulTmp = 0;
        }
    }
    else
    {
        ulTmp -= ulWIndex;
        ulTmp = RECM_INDEX_MAX - ulTmp;
    }

    if (ulRIndex != ulTmp)
    {
        RECM_WRITERECINDEX(pstSFlashInfo->pulRIndexId, &ulTmp);
    }

    return;
}
#endif
/*****************************************************************************
 * 函数名称: RECM_AddRecEx
 * 函数功能: 添加记录
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录: 返回索引号
 *           1. 2015-1-20 chenyue Create
 *****************************************************************************/
uint32 RECM_AddRecEx(uint8 ucRecType, uint8 *pData, uint16 usLen, uint8 *pErr)
{
    RECM_INFO_S *pstSFlashInfo = NULL;
    uint32 ulWIndex = 0;
    uint32 ulRIndex = 0;
    uint32 ulOffset = 0;
    uint32 ulTmp = 0;
    uint32 ulRet = 0;
    uint32 ulCnt;

    *pErr = FALSE;
    pstSFlashInfo = RECM_GetInfoAddr(ucRecType);

    if (NULL == pstSFlashInfo)
    {
        return ulRet;
    }

    OSSchedLock();
    RECM_READRECINDEX(pstSFlashInfo->pulWIndexId, &ulWIndex);
    RECM_READRECINDEX(pstSFlashInfo->pulRIndexId, &ulRIndex);

    ulOffset = RECM_GetAddr(pstSFlashInfo, ulWIndex);

    ulRet = ulWIndex;
    //RECM_UpdateReadIndex(pstSFlashInfo, ulWIndex, ulRIndex);

    if (RECM_INDEX_MAX <= ulWIndex)
    {
        ulWIndex = 0;
    }
    else
    {
        ulWIndex++;
    }

    RECM_WRITERECINDEX(pstSFlashInfo->pulWIndexId, &ulWIndex);
    OSSchedUnlock();

    if (pstSFlashInfo->ulUnitLen < usLen)
    {
        usLen = pstSFlashInfo->ulUnitLen;
    }

    DBG_LOG("[RECM] Add Rec Type:0x%02x WIndex:0x%08x RIndex:0x%08x Len:%d\r\n",
                                ucRecType,
                                ulRet,
                                ulRIndex,
                                usLen);

    if (RECM_BLOCK_SIZE < pstSFlashInfo->ulUnitLen)
    {
        ulTmp = pstSFlashInfo->ulUnitLen;
        ulTmp -= RECM_BLOCK_SIZE;
        ulCnt = ulTmp / RECM_BLOCK_SIZE;
        if (0 != (ulTmp % RECM_BLOCK_SIZE))
        {
            ulCnt += 1;
        }

        ulTmp = RECM_BLOCK_SIZE;
        while (ulCnt--)
        {
            FLASH_Erase(ulOffset + ulTmp);
            ulTmp += RECM_BLOCK_SIZE;
        }
    }

    DBG_LOG("[RECM] Offset:0x%08x", ulOffset);
    if (0 == (ulOffset % RECM_BLOCK_SIZE))
    {
        FLASH_Erase(ulOffset);
        DBG_LOG("[RECM] Erase");
    }

    if (0 != FLASH_OnlyWrite(ulOffset, pData, usLen))
    {
        DBG_LOG("[RECM] Write Ok");
        *pErr = TRUE;

        if (0 == (ulWIndex % pstSFlashInfo->ulBlockUnitNum))
        {
            ulOffset = RECM_GetAddr(pstSFlashInfo, ulWIndex);
            if (0 == (ulOffset % RECM_BLOCK_SIZE))
            {
                FLASH_Erase(ulOffset);
                DBG_LOG("[RECM] Erase Next Block");
            }
        }
        return ulRet;
    }

    return 0;
}

uint32 RECM_AddRec(uint8 ucRecType, uint8 *pData)
{
    uint8 err;

    return RECM_AddRecEx(ucRecType, pData, 0xffff, &err);
}

/*****************************************************************************
 * 函数名称: RECM_GetRecTotal
 * 函数功能: 返回记录条数
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2015-1-20 chenyue Create
 *****************************************************************************/
uint32 RECM_GetRecTotal(uint8 ucRecType)
{
    RECM_INFO_S *pstSFlashInfo = NULL;
    uint32 ulWIndex = 0;
    uint32 ulRIndex = 0;

    pstSFlashInfo = RECM_GetInfoAddr(ucRecType);

    if (NULL == pstSFlashInfo)
    {
        return 0;
    }

    RECM_READRECINDEX(pstSFlashInfo->pulWIndexId, &ulWIndex);
    RECM_READRECINDEX(pstSFlashInfo->pulRIndexId, &ulRIndex);

    if (ulWIndex >= ulRIndex)
    {
        ulWIndex -= ulRIndex;
    }
    else
    {
        ulWIndex = ulWIndex + (RECM_INDEX_MAX - ulRIndex) + 1;
    }
    DBG_PRINT(DBG_LEVEL_DEBUG, "[RECM] Rec Type:0x%2x Total:0x%08x\r\n",
                                ucRecType,
                                ulWIndex);

    return (ulWIndex);
}

/*****************************************************************************
 * 函数名称: RECM_GetOldestIndex
 * 函数功能: 返回存储的最早记录索引号(注意，当无记录时，返回无效)
 * 输入参数: uint8 ucRecType
 * 输出参数: void
 * 返回值  : uint32 此记录当前记录数不为0时返回值有效
 * 修改记录:
 *           1. 2015-1-20 chenyue Create
 *****************************************************************************/
uint32 RECM_GetOldestIndex(uint8 ucRecType)
{
    RECM_INFO_S *pstSFlashInfo = NULL;
    uint32 ulRIndex = 0;

    pstSFlashInfo = RECM_GetInfoAddr(ucRecType);

    if (NULL == pstSFlashInfo)
    {
        return 0;
    }

    RECM_READRECINDEX(pstSFlashInfo->pulRIndexId, &ulRIndex);

    DBG_PRINT(DBG_LEVEL_DEBUG, "[RECM] Rec Type:0x%02x RIndex:0x%08x\r\n",
                                ucRecType,
                                ulRIndex);

    return (ulRIndex);
}

/*****************************************************************************
 * 函数名称: RECM_GetLastetIndex
 * 函数功能: 返回存储的最新记录索引号(注意，当无记录时，返回无效)
 * 输入参数: uint8 ucRecType
 * 输出参数: void
 * 返回值  : uint32 此记录当前记录数不为0时返回值有效
 * 修改记录:
 *           1. 2015-1-20 chenyue Create
 *****************************************************************************/
uint32 RECM_GetLastIndex(uint8 ucRecType)
{
    RECM_INFO_S *pstSFlashInfo = NULL;
    uint32 ulWIndex = 0;

    pstSFlashInfo = RECM_GetInfoAddr(ucRecType);

    if (NULL == pstSFlashInfo)
    {
        return 0;
    }

    RECM_READRECINDEX(pstSFlashInfo->pulWIndexId, &ulWIndex);

    if (0 == ulWIndex)
    {
        ulWIndex = RECM_INDEX_MAX;
    }
    else
    {
        ulWIndex -= 1;
    }

    DBG_PRINT(DBG_LEVEL_DEBUG, "[RECM] Rec Type:0x%02x WIndex:0x%08x\r\n",
                                ucRecType,
                                ulWIndex);

    return (ulWIndex);
}

/*****************************************************************************
 * 函数名称: RECM_ReadRec
 * 函数功能: 根据记录类型和索引号获取记录内容
 * 输入参数: uint8 ucRecType 记录类型  ulRIndex 记录索引号
 * 输出参数: void
 * 返回值  : uint32
 * 修改记录:
 *           1. 2015-1-20 chenyue Create
 *****************************************************************************/
uint32 RECM_ReadRec(uint8 ucRecType, uint32 ulIndex, uint8 *pBuf, uint32 ulMaxLen)
{
    RECM_INFO_S *pstSFlashInfo = NULL;
    //uint32 ulWIndex = 0;
    //uint32 ulRIndex = 0;
    uint32 ulOffset = 0;

    pstSFlashInfo = RECM_GetInfoAddr(ucRecType);

    if (NULL == pstSFlashInfo)
    {
        return 0;
    }

    if (pstSFlashInfo->ulUnitLen < ulMaxLen)
    {
        ulMaxLen = pstSFlashInfo->ulUnitLen;
    }

    if (NULL == pBuf)
    {
        return 0;
    }

    #if 0
    RECM_READRECINDEX(pstSFlashInfo->pulWIndexId, &ulWIndex);
    RECM_READRECINDEX(pstSFlashInfo->pulRIndexId, &ulRIndex);

    if (ulWIndex >= ulRIndex)
    {
        if ((ulIndex >= ulWIndex) || (ulIndex < ulRIndex))
        {
            return 0;
        }
    }
    else
    {
        if ((ulIndex >= ulWIndex) && (ulIndex < ulRIndex))
        {
            return 0;
        }
    }
    #endif
    DBG_LOG("[RECM] Read Rec Type:0x%02x Index:0x%08x",
                                ucRecType,
                                ulIndex);

    ulOffset = RECM_GetAddr(pstSFlashInfo, ulIndex);

    FLASH_Read(ulOffset, pBuf, ulMaxLen);

    return (ulMaxLen);
}

/*****************************************************************************
 * 函数名称: RECM_ModData
 * 函数功能: 修改已有记录内容 只能修改bit位值为1的位置
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2015-2-13 chenyue Create
 *****************************************************************************/
uint8 RECM_ModData(uint8 ucRecType, uint32 ulIndex, uint32 ulOffset, uint8 *pBuf, uint32 ulMaxLen)
{
    RECM_INFO_S *pstSFlashInfo = NULL;
    uint32 ulWIndex = 0;
    uint32 ulRIndex = 0;
    uint32 ulAddr = 0;
    uint8 ucRet = FALSE;

    pstSFlashInfo = RECM_GetInfoAddr(ucRecType);

    if (NULL == pstSFlashInfo)
    {
        return ucRet;
    }

    if (pstSFlashInfo->ulUnitLen <= ulOffset)
    {
        return ucRet;
    }

    if (pstSFlashInfo->ulUnitLen < (ulOffset + ulMaxLen))
    {
        return ucRet;
    }

    if (NULL == pBuf)
    {
        return ucRet;
    }

    RECM_READRECINDEX(pstSFlashInfo->pulWIndexId, &ulWIndex);
    RECM_READRECINDEX(pstSFlashInfo->pulRIndexId, &ulRIndex);

    if (ulWIndex >= ulRIndex)
    {
        if ((ulIndex >= ulWIndex) || (ulIndex < ulRIndex))
        {
            return ucRet;
        }
    }
    else
    {
        if ((ulIndex >= ulWIndex) && (ulIndex < ulRIndex))
        {
            return ucRet;
        }
    }
    DBG_LOG("[RECM] Mod Rec Type:0x%02x Index:0x%08x",
                                ucRecType,
                                ulIndex);

    ulAddr = RECM_GetAddr(pstSFlashInfo, ulIndex);

    FLASH_OnlyWrite(ulAddr + ulOffset, pBuf, ulMaxLen);
    ucRet = TRUE;

    return ucRet;
}

/*****************************************************************************
 * 函数名称: RECM_UnitInit
 * 函数功能: 初始化索引值
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2016-9-20 chenyue Create
 *****************************************************************************/
void RECM_UnitInit(RECM_INFO_S *pstInfo)
{
#if 0
   
#else
    uint32 i;
    uint32 ulAddr;
    uint32 ulIndex = 0;
    uint8 aucBuf[4];
    uint16 usLen;

    i = pstInfo->ulTotalBlockNum * pstInfo->ulBlockUnitNum;
    for (ulIndex = 0; ulIndex < i; ulIndex++)
    {
        ulAddr = RECM_GetAddr(pstInfo, ulIndex);
        FLASH_Read(ulAddr, aucBuf, 2);
        PARAM_BaseTypeFromBuf(aucBuf, &usLen, 2);
        if ((0xffff == usLen) || (0 == usLen))
        {
            break;
        }
    }

#endif

    RECM_WRITERECINDEX(pstInfo->pulWIndexId, &ulIndex);
    RECM_WRITERECINDEX(pstInfo->pulRIndexId, &ulIndex);
    return;
}

void RECM_Init(void)
{
#if 1
    uint32 i;

    for (i = 0; i < RECM_INFO_MAXNUM; i++)
    {
        RECM_UnitInit((RECM_INFO_S *)&g_astSFlashInfo[i]);
        DBG_LOG("[RECM] Init Rec Type:0x%02x Index:0x%08x",g_astSFlashInfo[i].ucRecType, *g_astSFlashInfo[i].pulWIndexId);
    }
#else
  
#endif
    return;
}

/*****************************************************************************
**                            End of File
******************************************************************************/


