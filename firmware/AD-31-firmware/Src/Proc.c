/*****************************************************************************
 *
 * 文件描述: 协议处理
 * 输出参数: void
 * 修改记录:
 *
 *           1. 2016-7-12 chenyue Create
 *****************************************************************************/

/*****************************************************************************
**                          头文件包含和宏定义
******************************************************************************/
#include "user_comm.h"
#include "config.h"


uint32 PROC_MY_SN = 161019001;

#if 0
//uint8 g_aucProcRecvBuf[PROC_RXBUF_SIZE];    // ���ջ���

uint8 g_ucProcRxStep = 0;
uint8 g_ucProcRxCnt = 0;
uint8 g_ucProcRxMaxCnt = 0;
uint8 g_ucProcTimeDelay = 0;
#endif
uint8 g_aucProcSendBuf[PROC_TXBUF_SIZE];

/* 本机被指定查询标记--使用本机ID查询过就置位 */
uint8 g_ucProcLinkFlag = 0; // 0 未被查询 1 已被查询

/*****************************************************************************
 * 函数名称: PROC_SetCmd
 * 函数功能: 添加数据头
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2016-7-20 chenyue Create
 *****************************************************************************/
void PROC_SetCmd(uint8 cmd)
{
    g_aucProcSendBuf[PROC_OFFSET_HEAD] = 0x7E;

    g_aucProcSendBuf[PROC_OFFSET_ID] = (uint8)((PROC_MY_SN >> 24) & 0xff);
    g_aucProcSendBuf[PROC_OFFSET_ID + 1] = (uint8)((PROC_MY_SN >> 16) & 0xff);
    g_aucProcSendBuf[PROC_OFFSET_ID + 2] = (uint8)((PROC_MY_SN >> 8) & 0xff);
    g_aucProcSendBuf[PROC_OFFSET_ID + 3] = (uint8)((PROC_MY_SN >> 0) & 0xff);

    g_aucProcSendBuf[PROC_OFFSET_CMD] = cmd;

    g_aucProcSendBuf[PROC_OFFSET_LEN] = 0;
}

/*****************************************************************************
 * 函数名称: PROC_AddData
 * 函数功能: 往发送包添加参数
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2016-7-20 chenyue Create
 *****************************************************************************/
void PROC_AddData(uint8 ucId, uint8 ucDataLen, uint8 *pData)
{
    uint8 ucLen;
    uint8 i;

    ucLen = g_aucProcSendBuf[PROC_OFFSET_LEN];
    ucLen += PROC_OFFSET_DATASTART;

    g_aucProcSendBuf[ucLen++] = ucId;
    g_aucProcSendBuf[ucLen++] = ucDataLen;

    for (i = 0; i < ucDataLen; i++)
    {
        g_aucProcSendBuf[ucLen++] = pData[i];
    }

    ucLen -= PROC_OFFSET_DATASTART;
    g_aucProcSendBuf[PROC_OFFSET_LEN] = ucLen;

    return;
}

void PROC_AddBuf(uint8 *pData, uint8 ucDataLen)
{
    uint8 ucLen;
    uint8 i;

    ucLen = g_aucProcSendBuf[PROC_OFFSET_LEN];
    ucLen += PROC_OFFSET_DATASTART;

    for (i = 0; i < ucDataLen; i++)
    {
        g_aucProcSendBuf[ucLen++] = pData[i];
    }

    ucLen -= PROC_OFFSET_DATASTART;
    g_aucProcSendBuf[PROC_OFFSET_LEN] = ucLen;

    return;
}

/*****************************************************************************
 * 函数名称: PROC_Send
 * 函数功能: 整理数据并发送
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2016-7-20 chenyue Create
 *****************************************************************************/
void PROC_Send(void)
{
    uint8 ucLen;
    uint16 usCrc;

    ucLen = g_aucProcSendBuf[PROC_OFFSET_LEN];
    ucLen += PROC_OFFSET_DATASTART;

    //usCrc = 0;
    usCrc = PROC_Crc(g_aucProcSendBuf, ucLen, 0);
    g_aucProcSendBuf[ucLen] = (usCrc >> 8) & 0xff;
    ucLen++;
    g_aucProcSendBuf[ucLen] = (usCrc >> 0) & 0xff;
    ucLen++;
    g_aucProcSendBuf[ucLen] = 0x7E;
    ucLen++;

    PROC_SendBuf(g_aucProcSendBuf, ucLen);

    return;
}

#if 0
/*****************************************************************************
 * 函数名称: PROC_RxIrq
 * 函数功能: 串口接收中断处理
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2016-7-20 chenyue Create
 *****************************************************************************/
void PROC_RxIrq(uint8 ucValue)
{
    g_ucProcTimeDelay = PROC_TIME_RX;
    switch(g_ucProcRxStep)
    {
        case 0:
        {
            if (0x7E == ucValue)
            {
                g_ucProcRxStep++;
                g_aucProcRecvBuf[0] = ucValue;
                g_ucProcRxCnt = 1;
            }
            break;
        }
        case 1:
        {
            if (PROC_OFFSET_LEN == g_ucProcRxCnt)
            {
                g_ucProcRxMaxCnt = ucValue + 9;
                g_ucProcRxStep++;
            }

            g_aucProcRecvBuf[g_ucProcRxCnt] = ucValue;
            g_ucProcRxCnt++;
            break;
        }
        case 2:
        {
            if (g_ucProcRxMaxCnt == g_ucProcRxCnt)
            {
                g_ucProcTimeDelay = 0;
                g_ucProcRxStep = PROC_FLAG_RECVOK;
            }
            g_aucProcRecvBuf[g_ucProcRxCnt] = ucValue;
            g_ucProcRxCnt++;
            break;
        }
        default:
        {
            g_ucProcTimeDelay = 0;
            break;
        }
    }
}

/*****************************************************************************
 * 函数名称: PROC_TimeDeal
 * 函数功能: 时间相关处理10ms
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2016-7-20 chenyue Create
 *****************************************************************************/
void PROC_TimeDeal(void)
{
    if (0 != g_ucProcTimeDelay)
    {
        g_ucProcTimeDelay--;
        if (0 == g_ucProcTimeDelay)
        {
            g_ucProcRxCnt = 0;
        }
    }
}
#endif
/*****************************************************************************
 * 函数名称: PROC_Link
 * 函数功能: 在线检测
 * 输入参数: void
 * 输出参数: void
 * 返回值  : 0 不应答 0x02应答---广播指令，未登陆就应答
 * 修改记录:
 *           1. 2016-9-16 chenyue Create
 *****************************************************************************/
uint8 PROC_Link(uint8 *pData)
{
    uint8 aucBuf[4];
    uint32 ulMask;
    uint8 ucRet = PROC_BIT_NOACK;

    PARAM_BaseTypeFromBuf(pData, &ulMask, 4);

    if (0 == g_ucProcLinkFlag) // 未登陆
    {
        ulMask |= PROC_MY_SN;
        if (ulMask == PROC_MY_SN)
        {
            ucRet = PROC_BIT_ACK;
        }
    }
    else
    {
        ucRet = PROC_BIT_ACK;
    }

    aucBuf[0] = 0; // 设备类型
    PROC_AddBuf(aucBuf, 1);

    return ucRet;
}


/*****************************************************************************
 * 函数名称: PROC_ReadParam
 * 函数功能: 读取参数处理
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2016-7-20 chenyue Create
 *****************************************************************************/
void PROC_ReadParam(uint8 *pData)
{
    uint8 ucCnt;
    //uint8 ucTmp;
    uint8 ucId;
    //uint8 ucLen;
    uint8 aucBuf[TYPE_LSTRING_LEN];
    uint8 *pBuf;

    pBuf = pData;
    ucCnt = pBuf[0];
    pBuf++;

    aucBuf[0] = ucCnt;
    PROC_AddBuf(aucBuf, 1);
    //DBG_INFO("PROC_ReadParam");
    while (ucCnt--)
    {
        ucId = *pBuf;
        switch (ucId) {
            case PARAM_ID_DOMAIN:
            PROC_AddData(ucId, strlen(WorkParam.mqtt.MQTT_Server), (uint8*)WorkParam.mqtt.MQTT_Server);
            //memset(WorkParam.mqtt.MQTT_Server, 0, sizeof(WorkParam.mqtt.MQTT_Server));
            //PARAM_BaseTypeFromBuf((pBuf + 2), g_astParamInfo[ucTmp].pValue, ucLen);
            //memcpy (WorkParam.mqtt.MQTT_Server, (pBuf + 2), *(pBuf + 1));
            break;
            case  PARAM_ID_PORT:
            PARAM_BaseTypeToBuf(aucBuf, &WorkParam.mqtt.MQTT_Port, 2);
            PROC_AddData(ucId, 2, aucBuf);
            //PARAM_BaseTypeFromBuf((pBuf + 2), WorkParam.mqtt.MQTT_Port, 2);
            break;
            case  PARAM_ID_CLIENTID:
            PROC_AddData(ucId, strlen(WorkParam.mqtt.MQTT_ClientID), (uint8*)WorkParam.mqtt.MQTT_ClientID);
            break;
            break;
            default:
            break;
        }
        /*
        ucTmp = PARAM_SearchIndex(ucId);
        ucLen = g_astParamInfo[ucTmp].ucLen;
        if (4 >= ucLen)
        {
            PARAM_BaseTypeToBuf(aucBuf, g_astParamInfo[ucTmp].pValue, ucLen);
            PROC_AddData(ucId, ucLen, aucBuf);
        }
        else
        {
            PROC_AddData(ucId, ucLen, g_astParamInfo[ucTmp].pValue);
        }
        */
        pBuf++;
    }

    return;
}

void PROC_WriteParam(uint8 *pData)
{
    uint8 ucCnt;
    uint8 ucTmp;
    uint8 ucLen;
    uint8 ucLen2;
    //uint8 ucEepromAddr;
    uint8 *pBuf;

    pBuf = pData;
    ucCnt = pBuf[0];
    pBuf++;

    while (ucCnt--)
    {
        switch (*pBuf) {
            case PARAM_ID_DOMAIN:
            if(*(pBuf + 1) >= 32){
                return;
            }
            memset(WorkParam.mqtt.MQTT_Server, 0, sizeof(WorkParam.mqtt.MQTT_Server));
            //PARAM_BaseTypeFromBuf((pBuf + 2), g_astParamInfo[ucTmp].pValue, ucLen);
            memcpy (WorkParam.mqtt.MQTT_Server, (pBuf + 2), *(pBuf + 1));
            break;
            case  PARAM_ID_PORT:
            PARAM_BaseTypeFromBuf((pBuf + 2), &WorkParam.mqtt.MQTT_Port, 2);
            break;
            case  PARAM_ID_CLIENTID:
            if(*(pBuf + 1) >= 48){
                return;
            }
            memset(WorkParam.mqtt.MQTT_ClientID, 0, sizeof(WorkParam.mqtt.MQTT_ClientID));
            //PARAM_BaseTypeFromBuf((pBuf + 2), g_astParamInfo[ucTmp].pValue, ucLen);
            memcpy (WorkParam.mqtt.MQTT_ClientID, (pBuf + 2), *(pBuf + 1));
            break;
            break;
            default:
            break;
        }
        WorkParam_Save();
        ucTmp = PARAM_SearchIndex(*pBuf);
        ucLen = g_astParamInfo[ucTmp].ucLen;
        ucLen2 = *(pBuf + 1);
        if (4 >= ucLen)
        {
            PARAM_BaseTypeFromBuf((pBuf + 2), g_astParamInfo[ucTmp].pValue, ucLen);
        }
        else
        {
            memset(g_astParamInfo[ucTmp].pValue, 0, ucLen);
            if (ucLen2 >= ucLen)
            {
                return;
                /*
                ucLen2 = ucLen;

                if (0 != ucLen2)
                {
                    ucLen2 -= 1;
                }
                */
            }
            memcpy(g_astParamInfo[ucTmp].pValue, (pBuf + 2), ucLen2);
        }

        pBuf += *(pBuf + 1) + 2;
    }
    PARAM_SaveAll();
    ucTmp = 0;
    PROC_AddBuf(&ucTmp, 1);

    return;
}

#if 0
void PROC_GetSysRunTime(void)
{
    uint8 aucBuf[4];

    aucBuf[0] = (uint8)((g_ulSysBootSec >> 24) & 0xff);
    aucBuf[1] = (uint8)((g_ulSysBootSec >> 16) & 0xff);
    aucBuf[2] = (uint8)((g_ulSysBootSec >> 8) & 0xff);
    aucBuf[3] = (uint8)((g_ulSysBootSec >> 0) & 0xff);

    PROC_AddData(0x80, 4, aucBuf);
}
#endif

/*****************************************************************************
 * 函数名称: PROC_Deal
 * 函数功能: 协议处理
 * 输入参数: void
 * 输出参数: void
 * 返回值  : void
 * 修改记录:
 *           1. 2016-7-20 chenyue Create
 *****************************************************************************/
void PROC_Deal(uint8 *g_aucProcRecvBuf, uint8 g_ucProcRxCnt)
{
    uint16 usCrc;
    uint16 usTmp;
    uint32 ulId;
    uint8 *pData;
    uint8 cmd;
    uint8 ucFlagBit; // bit0 �Ƿ����� bit1 �Ƿ�Ӧ��
#if 0
    if (PROC_FLAG_RECVOK != g_ucProcRxStep)
    {
        return;
    }
#endif
    //  接收到数据包
    ucFlagBit = 0;
    // 校验处理
    usTmp = 0;
    usTmp = g_aucProcRecvBuf[g_ucProcRxCnt - 3];
    usTmp <<= 8;
    usTmp |= g_aucProcRecvBuf[g_ucProcRxCnt - 2];

    usCrc = PROC_Crc(g_aucProcRecvBuf, g_ucProcRxCnt - 3, 0);

    if (usCrc == usTmp)
    {
        /* 数据正确 */
        ulId = 0;
        ulId = g_aucProcRecvBuf[PROC_OFFSET_ID];
        ulId <<= 8;
        ulId |= g_aucProcRecvBuf[PROC_OFFSET_ID + 1];
        ulId <<= 8;
        ulId |= g_aucProcRecvBuf[PROC_OFFSET_ID + 2];
        ulId <<= 8;
        ulId |= g_aucProcRecvBuf[PROC_OFFSET_ID + 3];

        if (0 == ulId)
        {
            /* 广播数据 */
            if (0 == g_ucProcLinkFlag)
            {
                /* 未登陆 */
                ucFlagBit |= PROC_BIT_DEAL;
                ucFlagBit |= PROC_BIT_ACK; // ����ָ�Ӧ��
            }

            if (0 == g_aucProcRecvBuf[PROC_OFFSET_CMD])
            {
                ucFlagBit |= PROC_BIT_DEAL;
            }
        }

        if (PROC_MY_SN == ulId)
        {
            ucFlagBit |= PROC_BIT_DEALANDACK;
            g_ucProcLinkFlag = 1;
        }
    }

    if (0 != (ucFlagBit & PROC_BIT_DEAL))
    {
        cmd = g_aucProcRecvBuf[PROC_OFFSET_CMD];
        pData = &g_aucProcRecvBuf[PROC_OFFSET_DATASTART];

        if (0 != (ucFlagBit & PROC_BIT_ACK))
        {
            PROC_SetCmd(cmd);
        }

        switch (cmd)
        {
            case 0:
            {
                ucFlagBit &= ~PROC_BIT_ACK;
                g_ucProcLinkFlag = 0;
                break;
            }
            case 1:
            {
                ucFlagBit = PROC_Link(pData); // �˺������Ըı��Ƿ�Ӧ��
                break;
            }
            case 2: // �ֻ�������Ϣ
            {
                //PROC_ReadInfo(pData);
                break;
            }
            case 3: // 分机实时信息
            {
                //PROC_ReadRTStatus(pData);
                break;
            }
            case 4: // 读参数
            {
                PROC_ReadParam(pData);
                break;
            }
            case 5: // 写参数
            {
                PROC_WriteParam(pData);
                break;
            }

            case 6: // BL6523读参数
            {
                //PROC_ReadBL6523Param(pData);
                break;
            }
            case 7: // BL6523写参数
            {
                //PROC_WriteBL6523Param(pData);
                break;
            }
            case 8: /* 读BL6523配置 */
            {
                //PROC_ReadBl6523Cfg(pData);
                break;
            }
            case 9: /* 写BL6523配置 */
            {
                //PROC_WriteBl6523Cfg(pData);
                break;
            }
            case 0x0A: /* 清空BL6523配置 */
            {
                //PROC_ClrBl6523Cfg();
                break;
            }
            case 0x10:
            {
                //BL6523_Reset();
                PMET_ElecReset();
                break;
            }
            case 0x11: // 当前运行时间
            {
                //PROC_GetSysRunTime();
                break;
            }
            case 0x12:  //  设置开关状态
            {
                //PROC_SetSwitch(pData);
                break;
            }
            case 0x13:  // 设置用户额字电流值
            {
                //PROC_SetUserCurrent(pData);
                break;
            }
            case PMET_CMD_SET_PX:  // �����û����ֵ���ֵ
            {
                //PROC_SetUserCurrent(pData);
                break;
            }
            default:
            {
                ucFlagBit &= ~PROC_BIT_ACK; // ��Ӧ��
                break;
            }
        }

        if (0 != (ucFlagBit & PROC_BIT_ACK))
        {
            PROC_Send();
        }
    }

    // 恢复接收处理
    //g_ucProcRxStep = 0;

    return;
}

void PROC_Init(void)
{
    return;
}

/*****************************************************************************
**                            End of File
******************************************************************************/
