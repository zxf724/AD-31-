/*****************************************************************************
 *
 * 文件描述: 分机管理
 * 输出参数: void
 * 修改记录:
 *
 *           1. 2016-9-13 chenyue Create
 *****************************************************************************/

/*****************************************************************************
**                          头文件包含和宏定�?
******************************************************************************/
#include "user_comm.h"
#include "config.h"

uint8 g_ucPmetTerminalNum = 0;

//PMET_INFO_U *g_apstPmetInfo[PMET_TERMINAL_CNT] = {NULL};
//PMET_RTSTATUS_U *g_apstPmetRTStatus[PMET_TERMINAL_CNT] = {NULL};



uint8 g_aucPmetSendBuf[32]; // 数据发送缓

void *g_apPmetChnInfo[PMET_CHANNEL_CNT];   // 下标与通道对应  也就是说0无效



osMessageQId g_qPmetSend;
uint8 g_aucPmetStartChn[PMET_TERMINAL_CNT];     // �ֻ�ͨ����ʼ???



uint32 g_aulPmetElecInfo[RECM_UNITLEN_ELEC / 4] = {0}; // �����ۻ�???

//static uint8 PMET_EventDeal(uint8 *g_aucProcRecvBuf, uint8 g_ucProcRxCnt);
//static uint8 PMET_EventProc(void);

void PMET_GetBcdTime(uint8 aucTime[7])
{
    timeRTC_t stRTC;

    memset(aucTime, 0, 6);
    if (TRUE == RTC_ReadTime(&stRTC))
    {
        aucTime[0] = HEX_To_BCD(stRTC.year - 2000);
        aucTime[1] = HEX_To_BCD(stRTC.month);
        aucTime[2] = HEX_To_BCD(stRTC.date);
        aucTime[3] = HEX_To_BCD(stRTC.hours);
        aucTime[4] = HEX_To_BCD(stRTC.minutes);
        aucTime[5] = HEX_To_BCD(stRTC.seconds);
        aucTime[6] = HEX_To_BCD(stRTC.day);
    }

    return;
}

void PMET_BcdToDecTime(uint8 aucTime[7])
{
    uint32 i;

    for (i = 0; i < 7; i++)
    {
        aucTime[i] = BCD_To_HEX(aucTime[i]);
    }
}

/*****************************************************************************
 * 函数名称: PMET_SetCmd
 * 函数功能: 添加数据�?
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-7-20 chenyue Create
 *****************************************************************************/
void  PMET_SetCmd(uint8_t *pSendBuf, uint32_t sn, uint8_t cmd, uint8_t address)
{
    pSendBuf[PMET_OFFSET_HEAD] = 0x7E;
    pSendBuf[PMET_OFFSET_ADD] = address;
    pSendBuf[PMET_OFFSET_ID] = (uint8)((sn >> 24) & 0xff);
    pSendBuf[PMET_OFFSET_ID + 1] = (uint8)((sn >> 16) & 0xff);
    pSendBuf[PMET_OFFSET_ID + 2] = (uint8)((sn >> 8) & 0xff);
    pSendBuf[PMET_OFFSET_ID + 3] = (uint8)((sn >> 0) & 0xff);

    pSendBuf[PMET_OFFSET_CMD] = cmd;

    pSendBuf[PMET_OFFSET_LEN] = 0;
}

/*****************************************************************************
 * 函数名称: PMET_AddData
 * 函数功能: 往发送包添加参数
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-7-20 chenyue Create
 *****************************************************************************/
void PMET_AddData(uint8 *pSendBuf, uint8 ucId, uint8 ucDataLen, uint8 *pData)
{
    uint8 ucLen;
    uint8 i;

    ucLen = pSendBuf[PMET_OFFSET_LEN];
    ucLen += PMET_OFFSET_DATASTART;

    pSendBuf[ucLen++] = ucId;
    pSendBuf[ucLen++] = ucDataLen;

    for (i = 0; i < ucDataLen; i++)
    {
        pSendBuf[ucLen++] = pData[i];
    }

    ucLen -= PMET_OFFSET_DATASTART;
    pSendBuf[PMET_OFFSET_LEN] = ucLen;

    return;
}

void PMET_AddBuf(uint8 *pSendBuf, uint8 *pData, uint8 ucDataLen)
{
    uint8 ucLen;
    uint8 i;

    ucLen = pSendBuf[PMET_OFFSET_LEN];
    ucLen += PMET_OFFSET_DATASTART;

    for (i = 0; i < ucDataLen; i++)
    {
        pSendBuf[ucLen++] = pData[i];
    }

    ucLen -= PMET_OFFSET_DATASTART;
    pSendBuf[PMET_OFFSET_LEN] = ucLen;

    return;
}

/*****************************************************************************
 * 函数名称: PMET_Send
 * 函数功能: 整理数据并发�?
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-7-20 chenyue Create
 *****************************************************************************/
void PMET_Send(uint8 *pSendBuf)
{
    uint8 ucLen;
    uint16 usCrc;

    ucLen = pSendBuf[PMET_OFFSET_LEN];
    ucLen += PMET_OFFSET_DATASTART;

    //usCrc = 0;
    usCrc = CRC_16(0, pSendBuf, ucLen);
    pSendBuf[ucLen] = (usCrc >> 8) & 0xff;
    ucLen++;
    pSendBuf[ucLen] = (usCrc >> 0) & 0xff;
    ucLen++;
    pSendBuf[ucLen] = 0x7E;
    ucLen++;
    //DBG_LOG("[PMET] Data Send");
    //UART_SendData(3, pSendBuf, ucLen);
    //DBG_PRINTBUF(DBG_LEVEL_DEBUG, "[PMET] send:", pSendBuf, ucLen);
    UART_SendData(PMET_UART_PORT, pSendBuf, ucLen);
    
    //DBG_PRINTBUF(DBG_LEVEL_DEBUG, "[PMET] Send", pSendBuf, ucLen);
    //DBG_PRINTBUF(DBG_LEVEL_LOG, "[PMET] Send", pSendBuf, ucLen);

    return;
}

/*****************************************************************************
 * 函数名称: PMET_SendQ
 * 函数功能: 将发送数据加入到发送队列中 -- 缓存由调用方释放
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : true
 * 修改记录:
 *           1. 2016-9-19 chenyue Create
 *****************************************************************************/
uint8 PMET_SendQ(uint8 *pSendBuf)
{
    uint8 ucLen;
    uint16 usCrc;
    uint8 *pData;
    uint8 ucRet = TRUE;

    ucLen = pSendBuf[PMET_OFFSET_LEN];
    ucLen += PMET_OFFSET_DATASTART;

    //usCrc = 0;
    usCrc = CRC_16(0, pSendBuf, ucLen);
    pSendBuf[ucLen] = (usCrc >> 8) & 0xff;
    ucLen++;
    pSendBuf[ucLen] = (usCrc >> 0) & 0xff;
    ucLen++;
    pSendBuf[ucLen] = 0x7E;
    ucLen++;

    //DBG_LOG("[PMET] Add Buf SendQ Len:%d", ucLen);

    pData = pvPortMalloc(ucLen + 2);
    if (NULL != pData)
    {
        pData[0] = 0;
        pData[1] = ucLen;
        memcpy(&pData[2], pSendBuf, ucLen);
        if (osOK != osMessagePut(g_qPmetSend, (uint32)pData, 1000))
        {
            ucRet = FALSE;
            vPortFree(pData);
        }
    }

    return ucRet;
}

/*****************************************************************************
 * 函数名称: PMET_GetAndSendQ
 * 函数功能: 查询发送队列并执行发送处�?
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-9-19 chenyue Create
 *****************************************************************************/
uint8 PMET_GetAndSendQ(void)
{
    osEvent evt;
    uint8 *pData;
    uint8 ucLen;
    uint8 ucRet = FALSE;
    uint32 sn;
    while(1){
        evt = osMessageGet(g_qPmetSend, 20);
        if (evt.status == osEventMessage)
        {
            pData = evt.value.p;
            ucLen = pData[1];
            //osDelay(50);
            UART_SendData(PMET_UART_PORT, &pData[2], ucLen);
            UART_SendData(3, &pData[2], ucLen);
            PARAM_BaseTypeFromBuf(&pData[PMET_OFFSET_ID + 2], &sn, 4);
            ///DBG_PRINTBUF(DBG_LEVEL_INFO, "[PMET] QSend", &pData[2], ucLen);
            PMET_WaitAck(sn, pData[PMET_OFFSET_CMD + 2], pData[PMET_OFFSET_ADD + 2]);
            vPortFree(pData);
            ucRet = TRUE;
        }
        else{
            break;
        }
    }
    return ucRet;
}


/*****************************************************************************
 * 函数名称: PMET_GetChannelLineStatus
 * 函数功能: 获取通道在线状�?
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-9-19 chenyue Create
 *****************************************************************************/
uint8 PMET_GetChannelLineStatus(uint8 ucChannel)
{
    //uint8 i;
    uint8 ucStatus = FALSE;

    if ((1 <= ucChannel) && (PMET_CHANNEL_CNT > ucChannel))
    {
        if (NULL != g_apPmetChnInfo[ucChannel])
        {
            if (1 == ucChannel)
            {
                if (0 != ((PMET_CHANNELINFO_S *)g_apPmetChnInfo[ucChannel])->online)
                {
                    ucStatus = TRUE;
                }
            }
            else
            {
                if (0 != ((PMET_CHANNELINFO_S *)g_apPmetChnInfo[ucChannel])->online)
                {
                    ucStatus = TRUE;
                }
            }
        }
    }

    return ucStatus;
}





/*****************************************************************************
 * 函数名称: PMET_GetChnInfo
 * 函数功能: 获取分路开关信�?
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-9-19 chenyue Create
 *****************************************************************************/
PMET_CHANNELINFO_S *PMET_GetChnInfo(uint8 ucChannel)
{
    if ((1 <= ucChannel) && (ucChannel < PMET_CHANNEL_CNT))
    {
        return ((PMET_CHANNELINFO_S *)g_apPmetChnInfo[ucChannel]);
    }

    return NULL;
}

/*****************************************************************************
 * 函数名称: PMET_SetChannelSwitch
 * 函数功能: 远程控制开�?
 * 输入参数: ucChannel �?1开�? 2是分路开�?
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-9-19 chenyue Create
 *****************************************************************************/
void PMET_SetChannelSwitch(uint8_t address,uint32_t sn, uint8_t ucSwitch, uint8_t Cmd)
{
    uint8_t *pData;

    uint8_t aucBuf[5];
    uint32_t  switchset;    
    pData = pvPortMalloc(32);
    if (pData != NULL){
        PMET_SetCmd(pData, sn, Cmd, address);
        aucBuf[0] = 0;
        if (0 != ucSwitch){
            switchset = ((uint32_t)1 << (address - 1));
        }
        else{
            switchset = ((uint32_t)0 << (address - 1));
        }
        PARAM_BaseTypeToBuf(&aucBuf[1], &switchset, 4);
        PMET_AddBuf(pData, aucBuf, 5);
        PMET_SendQ(pData);
        vPortFree(pData);
    }
    
}
/*****************************************************************************

 *           1. 2016-9-19 chenyue Create
 *****************************************************************************/
void PMET_SetChnParam(uint8_t ucChannel, uint32_t sn, uint32_t para,uint8_t Cmd)
{
    uint8_t *pData;
    uint8_t onlineStatus;
    uint8_t aucBuf[8];
    onlineStatus = PMET_GetChannelLineStatus(ucChannel);
    if (0 != onlineStatus)
    {
        pData = pvPortMalloc(32);
        if (pData != NULL){
            
            DBG_LOG("[PROC] Set RatedCurrend Cmd:%d ucChannel:%d", Cmd, ucChannel);
            PMET_SetCmd(pData, sn, Cmd, ucChannel);
            aucBuf[0] = 0;
            PARAM_BaseTypeToBuf(&aucBuf[1], &para, 4);
            PMET_AddBuf(pData, aucBuf, 5);
            PMET_SendQ(pData);
            vPortFree(pData);
        }
    }
}



/*****************************************************************************
 * 函数名称: PMET_ProcDeal
 * 函数功能: 接收数据处理
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-9-18 chenyue Create
 *****************************************************************************/
void PMET_ProcDeal(uint32_t sn, uint8_t cmd, uint8_t *pData, uint8_t ucLen,uint8_t address)
{
    
    uint16_t usOffset = 0;
    //float fTmp;
    PMET_CHANNELINFO_S *channelinfo;
    /* 解析 */
    if (PMET_TERMINAL_CNT > address){
        switch (cmd){
            case PMET_CMD_ONLINE:{  
                if (NULL == g_apPmetChnInfo[address])
                {
                    g_apPmetChnInfo[address] = pvPortMalloc(sizeof(PMET_CHANNELINFO_S));
                    memset(g_apPmetChnInfo[address],0x00,sizeof(PMET_CHANNELINFO_S));
                }
                if (NULL != g_apPmetChnInfo[address])
                {
                    channelinfo = g_apPmetChnInfo[address];
                    channelinfo->sn = sn;
                } 
            }
            break;
            case PMET_CMD_INFO:{                    
                if (NULL == g_apPmetChnInfo[address])
                {
                    g_apPmetChnInfo[address] = pvPortMalloc(sizeof(PMET_CHANNELINFO_S));
                    memset(g_apPmetChnInfo[address],0x00,sizeof(PMET_CHANNELINFO_S));
                }
                if (NULL != g_apPmetChnInfo[address])
                {
                    channelinfo->sn = sn;
                    channelinfo = g_apPmetChnInfo[address];
                    channelinfo->online = 1;
                    channelinfo->version = pData[usOffset++];
                    channelinfo->type = pData[usOffset++];
                    channelinfo->limittemp = pData[usOffset++];
                    channelinfo->limitleakage = pData[usOffset++];
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(channelinfo->limitvoltageh), 4);
                    usOffset += 4;
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(channelinfo->limitvoltagel), 4);
                    usOffset += 4;
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(channelinfo->limitcurrent), 4);
                    usOffset += 4;
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(channelinfo->limitpower), 4);  
                }
            }
            break;
            case PMET_CMD_RTSTATUS:{
                if (NULL == g_apPmetChnInfo[address]){
                    g_apPmetChnInfo[address] = pvPortMalloc(sizeof(PMET_CHANNELINFO_S));
                    memset(g_apPmetChnInfo[address],0x00,sizeof(PMET_CHANNELINFO_S));
                }
                if (NULL != g_apPmetChnInfo[address])
                {
                    uint16_t newevent = 0;
                    channelinfo->sn = sn;
                    channelinfo = g_apPmetChnInfo[address]; 
                    channelinfo->online = 1;               
                    channelinfo->type = pData[usOffset++];                    
                    channelinfo->switchstatus = pData[usOffset++];
                    channelinfo->thunderstatus = pData[usOffset++];
                    channelinfo->temperature = pData[usOffset++];
                    channelinfo->leakage = pData[usOffset++];
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(newevent), 2);
                    usOffset += 2;
                    DBG_DBG("[PMET] newevent:%d channelinfo->error:%d\r)", newevent, channelinfo->error);
                    Publish_Event(address, sn, newevent, &(channelinfo->error), channelinfo->switchstatus);
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(channelinfo->powerfactor), 2);
                    usOffset += 2;
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(channelinfo->voltage[0]), 4);
                    usOffset += 4;
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(channelinfo->current[0]), 4);
                    usOffset += 4;
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(channelinfo->powerp), 2);
                    usOffset += 2;
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(channelinfo->powerq), 2);  
                    usOffset += 2;
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(channelinfo->powers), 2);  
                    usOffset += 2;
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(channelinfo->energyp), 4);  
                    usOffset += 4;
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(channelinfo->energyq), 4);  
                    usOffset += 4;
                    PARAM_BaseTypeFromBuf(&pData[usOffset], &(channelinfo->energys), 4); 
                                      
                }
                break;
            }
            default:
            {
                if (NULL == g_apPmetChnInfo[address])
                {
                    g_apPmetChnInfo[address] = pvPortMalloc(sizeof(PMET_CHANNELINFO_S));
                    memset(g_apPmetChnInfo[address],0x00,sizeof(PMET_CHANNELINFO_S));
                }
                if (NULL != g_apPmetChnInfo[address])
                {
                    channelinfo = g_apPmetChnInfo[address];
                    channelinfo->sn = sn;
                } 
                break;
            }
        }
    }
}

/*****************************************************************************
 * 函数名称: FMET_WaitAck
 * 函数功能: 等待应答并处�?
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : 0 无数�? 1 正确 2 有数据，但CRC不正�?
 * 修改记录:
 *           1. 2016-9-13 chenyue Create
 *****************************************************************************/
uint8 PMET_WaitAck(uint32_t sn, uint8_t cmd, uint8_t address)
{
    uint32_t ulStartTime;
    uint32_t ulAckSn;
    uint8_t *pData;
    uint16_t usCrc, usRCrc;
    uint8_t ucLen;
    uint8_t ucFlag = 0;
    ulStartTime = HAL_GetTick();
    while (120 >= (HAL_GetTick() - ulStartTime)){
        ucLen = UART_DataSize(PMET_UART_PORT);
        if ((0 != ucLen) && (UART_GetDataIdleTicks(PMET_UART_PORT) > 10)){
            DBG_PRINT(DBG_LEVEL_DEBUG, "[PMET] Recv Len:%d", ucLen);
            if (ucLen > 64)
            {
                ucLen = 64;
            }
            ucFlag = 2; // 接收

            pData = pvPortMalloc(64);

            if (NULL != pData){
                // 7E 09 95 E3 51 01 01 01 4A 05 7E
                UART_ReadData(PMET_UART_PORT, pData, ucLen);
                DBG_PRINTBUF(DBG_LEVEL_DEBUG, "[PMET] Recv:", pData, ucLen);
                usCrc = CRC_16(0, pData, ucLen - 3);

                usRCrc = 0;
                usRCrc |= pData[ucLen - 3];
                usRCrc <<= 8;
                usRCrc |= pData[ucLen - 2];
                if ((usCrc == usRCrc) && (cmd == pData[PMET_OFFSET_CMD])){
                    PARAM_BaseTypeFromBuf(&pData[PMET_OFFSET_ID], &ulAckSn, 4);
                    if ((0 == sn) || (sn == ulAckSn) 
                    || pData[PMET_OFFSET_ADD] == address || pData[PMET_OFFSET_ADD] == 0 ){
                        PMET_ProcDeal(ulAckSn, cmd, &pData[PMET_OFFSET_DATASTART], pData[PMET_OFFSET_LEN], address);
                        ucFlag = 1;
                    }
                }
                else{
                    DBG_LOG("[PMET] Recv Len:%d Err RCrc(0x%04x) Crc(0x%04x)", ucLen, usRCrc, usCrc);
                    DBG_PRINTBUF(DBG_LEVEL_INFO, "[PMET] Recv:", pData, ucLen);
                }
                vPortFree(pData);
            }
            break;
        }
        osDelay(2);
    }

    //DBG_LOG("[PMET] Wait Ack %s", (TRUE == ucFlag)?"OK":"TIME OUT");

    return ucFlag;
}

/*****************************************************************************
 * 函数名称: PMET_Clear
 * 函数功能: 从机清除在线标记
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-9-18 chenyue Create
 *****************************************************************************/
void PMET_Clear()
{
    uint8 aucBuf[4];
    uint32 ulTmp;

    PMET_SetCmd(g_aucPmetSendBuf, 0, PMET_CMD_CLEAR,0x00);
    ulTmp = 0;
    PARAM_BaseTypeToBuf(aucBuf, &ulTmp, 4);
    PMET_AddBuf(g_aucPmetSendBuf, aucBuf, 4);
    osDelay(200);
    PMET_Send(g_aucPmetSendBuf);
    osDelay(200);
    PMET_Send(g_aucPmetSendBuf);
    //osDelay(6000);   
    return;
}



void PMET_PrintTerminalInfo(void)
{
   
}



/*****************************************************************************
 * 函数名称: PMET_SearchNewTerminal
 * 函数功能: 搜索并标记新增终�?
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : 新增终端个数
 * 修改记录:
 *           1. 2016-9-18 chenyue Create
 *****************************************************************************/
uint8 PMET_SearchNewTerminal(void)
{
    uint8 i;
  
    uint8 ucNewCnt = 0;

    osDelay(100);
    DBG_LOG("[PMET] Chk New Terminal");
    for (i = 1; i < PMET_CHANNEL_CNT; i++){  
        PMET_SetCmd(g_aucPmetSendBuf, 0xFFFFFFFF, PMET_CMD_ONLINE, i);
        PMET_Send(g_aucPmetSendBuf);
        PMET_WaitAck(0xFFFFFFFF, PMET_CMD_ONLINE, i);
        ucNewCnt++;         
        osDelay(100);
    }
    
    DBG_PRINT(DBG_LEVEL_DEBUG, "[PMET] Query New Terminal");


    return ucNewCnt;
}

/*****************************************************************************
 * 函数名称: PMET_SortTerminalSn
 * 函数功能: 排序并保存新的终端SN列表
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-9-18 chenyue Create
 *****************************************************************************/
void PMET_SortTerminalSn(void){   
    return;
}



/*****************************************************************************
 * 函数名称: PMET_ElecReset
 * 函数功能: 重置电量累积
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-12-19 chenyue Create
 *****************************************************************************/
void PMET_ElecReset(void)
{
    RECM_INFO_S *pstRecmInfo;

    DBG_LOG("[PMET] Elec Heap Reset");
    pstRecmInfo = RECM_GetInfoAddr(RECM_RECTYPE_ELEC);
    memset((uint8 *)g_aulPmetElecInfo, 0, RECM_UNITLEN_ELEC);
    *pstRecmInfo->pulWIndexId = 0;
    *pstRecmInfo->pulRIndexId = 0;

    g_aulPmetElecInfo[0] = 0x55aa55aa;
    RECM_AddRec(RECM_RECTYPE_ELEC, (uint8 *)g_aulPmetElecInfo);

    return;
}

/*****************************************************************************
 * 函数名称: PMET_MakeElecRec
 * 函数功能: 生成当前记录数据并存�?
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-9-26 chenyue Create
 *****************************************************************************/
void PMET_MakeElecRec(void)
{
    /* 检测数据有效�? */
    g_aulPmetElecInfo[0] = 0x55aa55aa; // 设置记录有效
    RECM_AddRec(RECM_RECTYPE_ELEC, (uint8 *)g_aulPmetElecInfo);

    DBG_LOG("[PMET] Make ElecRec Save Ok");

    return;
}
/*****************************************************************************
 * 函数名称: PMET_MakeElecReset
 * 函数功能: 电量初始�?
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-9-21 chenyue Create
 *****************************************************************************/
void PMET_MakeElecReset(void)
{
    memset((uint8 *)g_aulPmetElecInfo, 0, RECM_UNITLEN_ELEC);
    PMET_MakeElecRec();
    PMET_Clear();
    DBG_LOG("[PMET] Reset ElecRec Save Ok");
    return;
}
uint32 *PMET_GetElecInfoAddr(uint8 ucChn)
{
    uint32 ulOffset;
    ulOffset = ucChn * 4;
    return &g_aulPmetElecInfo[ulOffset];
}

/*****************************************************************************
 * 函数名称: PMET_ElecHeapInit
 * 函数功能: 电量累计功能初始�?
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-9-21 chenyue Create
 *****************************************************************************/
void PMET_ElecHeapInit(void)
{
    uint8 i;
    uint16 usOffset;
    PMET_CHANNELINFO_S *pstchninfo;
    RECM_INFO_S *pstRecmInfo;
    uint32 ulWIndex;
    uint32 ulRIndex;

    pstRecmInfo = RECM_GetInfoAddr(RECM_RECTYPE_ELEC);

    memset((uint8 *)g_aulPmetElecInfo, 0, RECM_UNITLEN_ELEC);
    ulWIndex = *pstRecmInfo->pulWIndexId;

    if (0 != ulWIndex)
    {
        ulRIndex = *pstRecmInfo->pulRIndexId;
        if (ulWIndex == ulRIndex)
        {
            ulRIndex -= 1;
        }
        RECM_ReadRec(RECM_RECTYPE_ELEC, ulRIndex, (uint8 *)g_aulPmetElecInfo, RECM_UNITLEN_ELEC);
    }
    DBG_LOG("[PMET] Elec Heap Init W:0x%08x R:0x%08x", ulWIndex, ulRIndex);

    if (0x55aa55aa != g_aulPmetElecInfo[0])
    {
        PMET_ElecReset();
        DBG_LOG("[PMET] Elec Heap Init Reset");
        //memset((uint8 *)g_aulPmetElecInfo, 0, RECM_UNITLEN_ELEC);
    }
    //g_aulPmetElecInfo[0] = 0x55aa55aa;

    for (i = 1; i < PMET_CHANNEL_CNT; i++)
    {
        pstchninfo = PMET_GetChnInfo(i);
        if (NULL != pstchninfo)
        {
            usOffset = (i * 3) + 1;

            DBG_LOG("[PMET] Chn%d Elec Info:%d %d %d\r\n",
            i,
            g_aulPmetElecInfo[usOffset + 0], // month
            g_aulPmetElecInfo[usOffset + 1], // last
            g_aulPmetElecInfo[usOffset + 2],// day
            g_aulPmetElecInfo[usOffset + 3]);// day
        }
    }

    return;
}
/*****************************************************************************
 * 函数名称: PMET_ScanfTime
 * 函数功能:  处理定时动作
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : 0 不处�? 1开 2�?
 * 修改记录:
 *           1. 2016-9-21 chenyue Create
 *****************************************************************************/
uint8 PMET_ScanfRtc(uint8_t* aucTime, TIMING_S *pstSwitchTime)
{
    //uint8 i;
   
    //uint8 ucSel = 0;

    if ((0 == pstSwitchTime->mode) || ( pstSwitchTime->num > 5))
    {
        return 0;
    }    
    if(1 == pstSwitchTime->mode){
        if(RTC_ReadTick() - 28800 == pstSwitchTime->startTick){
            DBG_LOG("[PMET] RTC_ReadTick:%d %d %d",
                        RTC_ReadTick() - 28800,
                        pstSwitchTime->startTick,
                        pstSwitchTime->endTick
                        );
            return 1;
        }
        if(RTC_ReadTick() - 28800 == pstSwitchTime->endTick){
            DBG_LOG("[PMET] RTC_ReadTick:%d %d %d",
                        RTC_ReadTick() - 28800,
                        pstSwitchTime->startTick,
                        pstSwitchTime->endTick
                        );
            return 2;
        }
        return 0;
    }    
    return 0;
}
/*****************************************************************************
 * 函数名称: PMET_ScanfTime
 * 函数功能:  处理定时动作
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : 0 不处�? 1开 2�?
 * 修改记录:
 *           1. 2016-9-21 chenyue Create
 *****************************************************************************/
uint8 PMET_ScanfTime(uint8_t* aucTime, TIMING_S *pstSwitchTime)
{
    uint8 i;
    //static uint8_t minStatic = 0;
    //uint8 ucSel = 0;

    if ((0 == pstSwitchTime->mode) || ( pstSwitchTime->num > 5))
    {
        return 0;
    }    
    if((RTC_ReadTick() - 28800) >= pstSwitchTime->startTick && 
       (RTC_ReadTick() - 28800) <= pstSwitchTime->endTick){
        
        if(2 == pstSwitchTime->mode){
            if(pstSwitchTime->week[aucTime[6]] == 0){
                return 0;
            }
            for(i = 0; i < 2; i++){
                if (aucTime[3 + i] != pstSwitchTime->startTime[i])
                {
                    break;
                }
            }
            if (aucTime[3] == pstSwitchTime->startTime[0] && aucTime[4] == pstSwitchTime->startTime[1]){
                return 1;
            }
            if (aucTime[3] == pstSwitchTime->endTime[0] && aucTime[4] == pstSwitchTime->endTime[1]){
                return 2;
            }
            DBG_LOG("[PMET] ScanfTime time:%02x %02x %02x %02x %02x %02x %02x",
                            aucTime[0],
                            aucTime[1],
                            aucTime[2],
                            aucTime[3],
                            aucTime[4],
                            aucTime[5],
                            aucTime[6]
                            );
            DBG_LOG("[PMET] ScanfTime time:%02x %02x %02x %02x\r",
                pstSwitchTime->startTime[0],
                pstSwitchTime->startTime[1],
                pstSwitchTime->endTime[0],
                pstSwitchTime->endTime[1]
                );
        }
        else if(3 == pstSwitchTime->mode){
            if (aucTime[3] == pstSwitchTime->startTime[0] && aucTime[4] == pstSwitchTime->startTime[1]){
                return 1;
            }
        }
        else if(4 == pstSwitchTime->mode){
            if (aucTime[3] == pstSwitchTime->endTime[0] && aucTime[4] == pstSwitchTime->endTime[1]){
                return 2;
            }
        }
    }
    return 0;
}
static void PMET_TimeSwitch(uint8_t* aucTime){
    SWITCHTIMESET_S *pstSwitchTime;
    PMET_CHANNELINFO_S *pstchninfo;
    uint8_t ucRet;
    static uint8_t minStatic = 0;
    pstSwitchTime = pvPortMalloc(SWTIME_BUFSIZE_MAX);
    if (NULL != pstSwitchTime){
        FLASH_Read(SWTIME_ADDR_START, (uint8 *)pstSwitchTime, SWTIME_BUFSIZE_MAX);
        
        //-------------------------------
        for (uint8_t i = 1; i < PMET_CHANNEL_CNT; i++)
        {
            pstchninfo = PMET_GetChnInfo(i);
            if (NULL != pstchninfo)
            {
                //DBG_DBG("[PMET] Timer bracker i %d",i);
                for (uint8 j = 0; j < 5; j++){
                    ucRet = PMET_ScanfRtc(aucTime, &pstSwitchTime[i - 1].timingData[j]);\
                    if(ucRet == 0 && minStatic != aucTime[4]){
                        ucRet = PMET_ScanfTime(aucTime, &pstSwitchTime[i - 1].timingData[j]);
                    }
                    if (1 == ucRet){                        
                        DBG_DBG("[PMET] Timer Chn %d Open", i - 1);
                        PMET_SetChannelSwitch(i,pstchninfo->sn, 1, PMET_CMD_SET_SWITCH);                        
                    }
                    else if (2 == ucRet){
                        DBG_DBG("[PMET] Timer Chn %d Close", i - 1);
                        PMET_SetChannelSwitch(i, pstchninfo->sn, 0, PMET_CMD_SET_SWITCH);                        
                    }
                }
            }
        }
        minStatic = aucTime[4];
        vPortFree(pstSwitchTime);
    }
}
/*****************************************************************************
 * 函数名称: PMET_ClockDeal
 * 函数功能: 时间处理 -- 电量累积处理 -- 定时处理
 * 输入参数: void
 * 输出参数: void
 * 返回�?  : void
 * 修改记录:
 *           1. 2016-9-21 chenyue Create
 *****************************************************************************/
void PMET_ClockDeal(void)
{
    static uint8 g_ucPmetClockDealSec = 0xff;
    static uint8 g_ucPmetClockDealMin = 0xff;
    static uint8 g_ucPmetClockDealDay = 0xff;
    static uint8 g_ucPmetClockDealMonth = 0xff;
    uint8 aucTime[7];
    uint8 ucMonthFlag = 0;
    uint8 i;

    //uint32 ulSecCnt;
    //uint32 ulPower;
    //float fTmp;
    PMET_CHANNELINFO_S *pstchninfo;
    uint32 *pulElecInfo;

    PMET_GetBcdTime(aucTime);
    PMET_TimeSwitch(aucTime);
    if (0xff == g_ucPmetClockDealSec)
    {
        g_ucPmetClockDealSec = aucTime[5];
        g_ucPmetClockDealDay = aucTime[2];
        g_ucPmetClockDealMonth = aucTime[1];
        g_ucPmetClockDealMin = aucTime[4];

        return;
    }
    if (g_ucPmetClockDealMin != aucTime[4]){

        g_ucPmetClockDealMin = aucTime[4];

        uint8 uDayFlag = 0;
        if (g_ucPmetClockDealDay != aucTime[2])
        {
            /*  */
            uDayFlag = 1;
            DBG_LOG("[PMET] uDayFlag");
            g_ucPmetClockDealDay = aucTime[2];
            if (g_ucPmetClockDealMonth != aucTime[1])
            {
                /*  */
                g_ucPmetClockDealMonth = aucTime[1];
                ucMonthFlag = 1;
                DBG_LOG("[PMET] ucMonthFlag");
            }
            

            for (i = 1; i < PMET_CHANNEL_CNT; i++)
            {
                pstchninfo = PMET_GetChnInfo(i);
                if (NULL != pstchninfo)
                {
                    pulElecInfo = PMET_GetElecInfoAddr(i);                    
                    pulElecInfo[1] = pulElecInfo[0];
                    pulElecInfo[0] = pstchninfo->energyp;
                    if (0 != ucMonthFlag)
                    {
                        pulElecInfo[3] = pulElecInfo[2];
                        pulElecInfo[2] = pstchninfo->energyp;
                    }
                }
            }
            PMET_MakeElecRec(); //
            if(uDayFlag == 1){
                if(MQTT_IsConnected()){
                    //Publish_PowerMeter();
                }
            }
        }
        /*  */

    }
}

// ��ʼ���ֻ�SN
void PMET_TerminalSnInit(void){
    //uint32 i;
    //uint8 *pData;

}

// 所有在线分机检测后，与上次不一�? 重新保存
void PMET_TerminalSnSave(void)
{

    return;
}

void PMET_Task(void const *argument)
{
    uint8 i = 0;
    argument = argument;
    DBG_LOG("[PMET] Task Start");
    //PMET_TerminalSnInit();
    /* 初始化记录功�? */
    RECM_Init();   
    PMET_Clear();

    /* ------------搜索新分机------------- */
    PMET_SearchNewTerminal();
    PMET_SearchNewTerminal();
    //PMET_SearchNewTerminal(); //3次
    /* ------------排序------------ */
    //PMET_SortTerminalSn();
    //PMET_PrintTerminalInfo();
    

    /* 载入累计电量 */
    PMET_ElecHeapInit();
    /* ------------工作过程中，不再检测新分机------------ */
    g_ucPmetTerminalNum = 0;
    while (TRUE){
        PMET_ClockDeal();
        if (FALSE != PMET_GetAndSendQ()){
            osDelay(50);
        }
        else{
            i = 1;
        }
        osDelay(10);
        for (i = g_ucPmetTerminalNum; i < PMET_CHANNEL_CNT; i++){
            if (NULL != g_apPmetChnInfo[i]){
                break;
            }
        }
        static uint8 sortFlag = 0;
        g_ucPmetTerminalNum = i;
        if (PMET_CHANNEL_CNT <= i) {//轮询完毕
            g_ucPmetTerminalNum = 0;            
            if(sortFlag == 0 && MQTT_IsConnected()){
                sortFlag = 1;                
                CMD_Virtual("process info_updata"); //主动上传一次信息
                 
            }
        }
        else{            
            osDelay(200);
            PMET_CHANNELINFO_S *channelinfo;           
            channelinfo = g_apPmetChnInfo[g_ucPmetTerminalNum];
            if(channelinfo){
                if(sortFlag == 0){
                    PMET_SetCmd(g_aucPmetSendBuf, channelinfo->sn, 
                            PMET_CMD_INFO, g_ucPmetTerminalNum);
                    PMET_Send(g_aucPmetSendBuf);
                    PMET_WaitAck(channelinfo->sn, PMET_CMD_INFO, 
                             g_ucPmetTerminalNum);
                    osDelay(200);
                } 
                PMET_SetCmd(g_aucPmetSendBuf, channelinfo->sn, 
                            PMET_CMD_RTSTATUS, g_ucPmetTerminalNum); //获取实时信息
                PMET_Send(g_aucPmetSendBuf);
                PMET_WaitAck(channelinfo->sn, PMET_CMD_RTSTATUS, 
                             g_ucPmetTerminalNum);
                g_ucPmetTerminalNum++;
            }
            

        }
        static uint32_t searchtick = 0;
        if(TS_IS_OVER(searchtick, 86400000)){ //一天查询下分机
            sortFlag = 0;
            PMET_SearchNewTerminal();
        }
        //DBG_LOG("[PMET] PMET_Task\r");
        osDelay(500);
       // PMET_UserSendAndWaitAck();
    }
}
#if 0
void PMET_Console(int argc, char *argv[])
{
    uint32 addr = 0, ret = 0;

    argv++;
    argc--;
    if (strcmp(argv[0], "test") == 0) {
        argv++;
        argc--;
        if (strcmp(argv[0], "req") == 0) {
            addr = uatoi(argv[1]);
            //ret = Bracker_ReqAddress(&addr);
            if (ret) {
                //testaddr = addr;
            }
            DBG_LOG("meter test request device ret:%d, address:%u", ret, addr);
        } else if (strcmp(argv[0], "switch") == 0) {
            //ret = Bracker_Switch(testaddr, uatoi(argv[1]));
            DBG_LOG("test bracker switch ret:%d.", ret);
        }
        // if (strcmp(argv[0], "switch") == 0) {}
    }
}
#endif
void PMET_Init(void)
{
    uint32 i;

    UART_SetBaudrate(PMET_UART_PORT, PMET_UART_BDR);

    //CMD_ENT_DEF(PMET, PMET_Console);
    //Cmd_AddEntrance(CMD_ENT(PMET));

    osThreadDef(PMET, PMET_Task, PMET_TASK_PRIO, 0, PMET_TASK_STK_SIZE);
    osThreadCreate(osThread(PMET), NULL);

    for (i = 0; i < PMET_CHANNEL_CNT; i++)
    {
        g_apPmetChnInfo[i] = NULL;
    }

    /* 数据发送队�? */
    osMessageQDef(PMET_Send_Q, 12, void *);
    g_qPmetSend = osMessageCreate(osMessageQ(PMET_Send_Q), NULL);

    DBG_LOG("Power meter start.");
}


/*****************************************************************************
**                            End of File
******************************************************************************/
