/*****************************************************************************
 *
 * 文件描述: 参数处理
 * 输出参数: void
 * 修改记录:
 *
 *           1. 2016-7-25 chenyue Create
 *****************************************************************************/

/*****************************************************************************
**                          头文件包含和宏定义
******************************************************************************/
#include "user_comm.h"
#include "config.h"


uint8 g_ucParamMemMode = 0; //0大端，1小端

uint16 g_usParamIndexMax = 0;

const PARAM_INFO_S g_astParamInfo[] = {
    { 0, 0, 0, NULL },
    { PARAM_ID_INDEXMAX,    TYPE_UINT16_LEN,    GET_SYSCFG_MEMBER_ADDR(usParamIndexMax),   &g_usParamIndexMax },
    { PARAM_ID_CLIENTID,    TYPE_LSTRING_LEN,   GET_SYSCFG_MEMBER_ADDR(szClientId),        &g_szClientId},
    { PARAM_ID_DOMAIN,      TYPE_LSTRING_LEN,   GET_SYSCFG_MEMBER_ADDR(szDomain),          &g_szDomain },
    { PARAM_ID_PORT,        TYPE_UINT16_LEN,    GET_SYSCFG_MEMBER_ADDR(usPort),            &g_usPort },

    //{ PARAM_ID_ELECMONTH,    TYPE_UINT8_LEN,     GET_SYSCFG_MEMBER_ADDR(ucPmetElecHeapMonth),    &g_ucPmetElecHeapDay },
    //{ PARAM_ID_ELECYEAR,     TYPE_UINT8_LEN,     GET_SYSCFG_MEMBER_ADDR(ucPmetElecHeapYear),     &g_ucPmetElecHeapMonth },
    //{PARAM_RAMID_VOLTAGE2,  4, 0,    &g_aulVoltage[1]},
};

/*****************************************************************************
 * 函数名称: PARAM_ChkMemMode
 * 函数功能: 检测当前CPU大小端模式
 * 输入参数: void
 * 输出参数: void
 * 返回值  : uint8
 * 修改记录:
 *           1. 2014-12-19  Create
 *****************************************************************************/
uint8 PARAM_ChkMemMode(void)
{
    PARAM_TYPE_U stUParam;
    uint8 Ret = 0;

    stUParam.ulData = 0x11223344;

    if ((0x11 == stUParam.aucData[0])
        && (0x22 == stUParam.aucData[1])
        && (0x33 == stUParam.aucData[2])
        && (0x44 == stUParam.aucData[3])) {
        Ret = 1;
    } else {
        Ret = 0;
    }

    return Ret;
}

/*****************************************************************************
 * 函数名称: PARAM_GetMemMode
 * 函数功能: 获取当前内存存储模式
 * 输入参数: void
 * 输出参数: void
 * 返回值  : 0:小端 1:大端
 * 修改记录:
 *           1. 2014-12-17 chenyue Create
 *****************************************************************************/
uint8 PARAM_GetMemMode(void)
{
    return g_ucParamMemMode;
}

/*****************************************************************************
 * 函数名称: PARAM_BaseTypeToBuf
 * 函数功能: 根据硬件大小端模式填充
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2014-12-19  Create
 *****************************************************************************/
void PARAM_BaseTypeToBuf(uint8 *pBuf, void *pValue, uint32 ulLen)
{
    uint32 i;
    uint8 *pTmpBuf = (uint8 *)pValue;
    uint8 ucOffset;

    if (0 != ulLen) {
        if (1 == PARAM_GetMemMode()) {
            for (i = 0; i < ulLen; i++) {
                pBuf[i] = pTmpBuf[i];
            }
        } else {
            ucOffset = ulLen - 1;
            for (i = 0; i < ulLen; i++) {
                pBuf[i] = pTmpBuf[ucOffset];
                ucOffset--;
            }
        }
    }

    return;
}

/*****************************************************************************
 * 函数名称: PARAM_BaseTypeFromBuf
 * 函数功能: 根据硬件大小端模式填充
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2014-12-19  Create
 *****************************************************************************/
void PARAM_BaseTypeFromBuf(uint8 *pBuf, void *pValue, uint32 ulLen)
{
    uint32 i;
    uint8 *pTmpBuf = (uint8 *)pValue;
    uint8 ucOffset;

    if (0 != ulLen) {
        if (1 == PARAM_GetMemMode()) {
            for (i = 0; i < ulLen; i++) {
                pTmpBuf[i] = pBuf[i];
            }
        } else {
            ucOffset = ulLen - 1;
            for (i = 0; i < ulLen; i++) {
                pTmpBuf[ucOffset] = pBuf[i];
                ucOffset--;
            }
        }
    }

    return;
}

PARAM_T PARAM_SearchIndex(PARAM_T ucId)
{
    PARAM_T ucMaxCnt;
    PARAM_T i;

    ucMaxCnt = sizeof(g_astParamInfo) / sizeof(PARAM_INFO_S);

    for (i = 0; i < ucMaxCnt; i++) {
        if (ucId == g_astParamInfo[i].ucId) {
            break;
        }
    }

    if (i == ucMaxCnt) {
        return 0;
    }

    return i;
}

PARAM_T PARAM_SearchMemAddr(void *pValue)
{
    PARAM_T ucMaxCnt;
    PARAM_T i;

    ucMaxCnt = sizeof(g_astParamInfo) / sizeof(PARAM_INFO_S);

    for (i = 0; i < ucMaxCnt; i++) {
        if (pValue == g_astParamInfo[i].pValue) {
            break;
        }
    }

    if (i == ucMaxCnt) {
        return 0;
    }

    return i;
}

void PARAM_UpdateFlash(PARAM_T ucIndex)
{
    PARAM_TYPE_U stUParam;
    uint8 ucLen = 0;
    uint8 ucTmp;
    PARAM_T2 ucAddr;

    //uint8 i;
    ucLen = g_astParamInfo[ucIndex].ucLen;
    switch (ucLen) {
    case 0:
        {
            break;
        }
    case 1:
        {
            stUParam.ucData = ((uint8 *)g_astParamInfo[ucIndex].pValue)[0];
            //ucLen = 1;
            break;
        }
    case 2:
        {
            stUParam.usData = ((uint16 *)g_astParamInfo[ucIndex].pValue)[0];
            //ucLen= 2;
            break;
        }
    case 3:
        {

            break;
        }
    case 4:
        {
            stUParam.ulData = ((uint32 *)g_astParamInfo[ucIndex].pValue)[0];
            //ucLen = 4;
            break;
        }
    default:
        {
            if (TYPE_LSTRING_LEN >= ucLen) {
                memcpy(stUParam.aucData, (uint8 *)g_astParamInfo[ucIndex].pValue, ucLen);
            } else {
                ucAddr = g_astParamInfo[ucIndex].ucSaveAddr;
                if (0 != ucAddr) {
                    while (0 < ucLen) {
                        if (TYPE_LSTRING_LEN < ucLen) {
                            ucTmp = TYPE_LSTRING_LEN;
                        } else {
                            ucTmp = ucLen;
                        }
                        ucLen -= ucTmp;
                        memcpy(stUParam.aucData, (uint8 *)g_astParamInfo[ucIndex].pValue, ucTmp);
                        ucAddr += ucTmp;
                        PARAM_Write(ucAddr, stUParam.aucData, ucLen);
                    }
                }

                return;
            }
            break;
        }
    }

    if (TYPE_LSTRING_LEN >= ucLen) {
        ucAddr = g_astParamInfo[ucIndex].ucSaveAddr;
        if (0 != ucAddr) {
            PARAM_Write(ucAddr, stUParam.aucData, ucLen);
        }
    }

    return;
}

void PARAM_SaveFromMemAddr(void *pMemAddr)
{
    PARAM_T ucIndex;

    ucIndex = PARAM_SearchMemAddr(pMemAddr);
    PARAM_UpdateFlash(ucIndex);

    return;
}

void PARAM_UpdateRamData(PARAM_T ucIndex)
{
    PARAM_TYPE_U stUParam;
    uint8 ucLen = 0;
    uint8 ucTmp;
    uint32 ulOffset;
    PARAM_T2 ucAddr;

    //uint8 i;
    if (NULL == g_astParamInfo[ucIndex].pValue) {
        return;
    }

    ucLen = g_astParamInfo[ucIndex].ucLen;
    ucAddr = g_astParamInfo[ucIndex].ucSaveAddr;

    if (0 != ucAddr) {
        if (TYPE_LSTRING_LEN >= ucLen) {
           PARAM_Read(ucAddr, stUParam.aucData, ucLen);
        }

        switch (ucLen) {
        case 0:
            {
                break;
            }
        case 1:
            {
                ((uint8 *)g_astParamInfo[ucIndex].pValue)[0] = stUParam.ucData;
                break;
            }
        case 2:
            {
                ((uint16 *)g_astParamInfo[ucIndex].pValue)[0] = stUParam.usData;
                break;
            }
        case 3:
            {

                break;
            }
        case 4:
            {
                ((uint32 *)g_astParamInfo[ucIndex].pValue)[0] = stUParam.ulData;
                break;
            }
        default:
            {
                if (TYPE_LSTRING_LEN >= ucLen) {
                    memcpy((uint8 *)g_astParamInfo[ucIndex].pValue, stUParam.aucData, ucLen);
                } else {
                    ulOffset = 0;
                    while (0 < ucLen) {
                        if (TYPE_LSTRING_LEN < ucLen) {
                            ucTmp = TYPE_LSTRING_LEN;
                        } else {
                            ucTmp = ucLen;
                        }
                        ucLen -= ucTmp;
                        PARAM_Read(ucAddr, stUParam.aucData, ucTmp);
                        ucAddr += ucTmp;

                        memcpy((uint8 *)((uint32)g_astParamInfo[ucIndex].pValue + ulOffset), stUParam.aucData, ucTmp);
                        ulOffset += ucTmp;
                    }

                    return;
                }

                break;
            }
        }
    }
}

void PARAM_SaveAll(void)
{
    PARAM_T i;
    uint16 usIndexMax;
    //uint8 ucData;

    // 删除所有
    SFlash_EraseSectors(PARAM_SYSCONFIG_ADDR, SECTOR_PARAM_SIZE);

    /* 存入最新的内存变量 */
    usIndexMax = sizeof(g_astParamInfo);
    usIndexMax /= sizeof(PARAM_INFO_S);
    g_usParamIndexMax = usIndexMax;
    for (i = 0; i < usIndexMax; i++) {
        PARAM_UpdateFlash(i);
    }

    return;
}

void PARAM_Reset(void)
{
    PARAM_T i;

    g_usParamIndexMax = 0;
    i = PARAM_SearchMemAddr(&g_usParamIndexMax);
    PARAM_UpdateFlash(i);

    DBG_LOG("[PARAM] Reset...");

    osDelay(1000);
    NVIC_SystemReset();
}

void PARAM_Console(int argc, char *argv[])
{
    argv++;
    argc--;
    if (strcmp(argv[0], "RESET") == 0) {
        PARAM_Reset();
    }
}

/*****************************************************************************
 * 函数名称: PARAM_Init
 * 函数功能: 参数初始化
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2016-7-25 chenyue Create
 *****************************************************************************/
void PARAM_Init(void)
{
    PARAM_T i;
    PARAM_T usIndexMax;

    //DBG_LOG("[PARAM] IP:%s", g_szDomain);
    g_ucParamMemMode = PARAM_ChkMemMode();
    DBG_LOG("[PARAM] Init MemMode:%d", g_ucParamMemMode);

    CMD_ENT_DEF(PARAM, PARAM_Console);
    Cmd_AddEntrance(CMD_ENT(PARAM));

    usIndexMax = sizeof(g_astParamInfo);
    usIndexMax /= sizeof(PARAM_INFO_S);

    i = PARAM_SearchIndex(PARAM_ID_INDEXMAX);
    PARAM_UpdateRamData(i);

    if ((0 == g_usParamIndexMax) || (0xffff == g_usParamIndexMax) || (usIndexMax < g_usParamIndexMax)) {
        DBG_LOG("[PARAM] Invalid Index:%d(%d)", g_usParamIndexMax, usIndexMax);

        // 尽量不改变SN号
        i = PARAM_SearchIndex(PARAM_ID_CLIENTID);
        PARAM_UpdateRamData(i);
        if (0xff == g_szClientId[0])
        {
            strcpy(g_szClientId, "ID002");
        }
        PARAM_SaveAll();
    } else {
        /* 只读取有效索引--无效索引使用默认值 */
        for (i = 0; i < g_usParamIndexMax; i++) {
            PARAM_UpdateRamData(i);
        }
    }

    return;
}

/*****************************************************************************
**                            End of File
******************************************************************************/
