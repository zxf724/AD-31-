/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file mqtt_conn.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.12.20
 * @brief MQTT连接管理函数文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _MQTT_CONN_H
#define _MQTT_CONN_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"
#include "MQTTPacket.h"
#include "MQTTClient.h"
#define     MQTT_TYPE           1//0:MQTT,1:TCP
/* Exported define -----------------------------------------------------------*/
/*MQTT调试使能*/
#define MQTT_DEBUG                      2

/*协议处理缓存长度*/
#define MQTT_TX_BUFF_SIZE               1024    
#define MQTT_RX_BUFF_SIZE               1024

/*MQTT 响应超时时间*/
#define MQTT_TIMEOUT_DEF                10000

/*MQTT的心跳包间隔,实际发送间隔为设置值的一半，单位秒*/
#define MQTT_PING_INVT_DEF              80
#define MQTT_DATA_INVT_DEF              1200 //主动上报间隔时间
/*阿里云接入使能*/
#define MQTT_ALIYUN_EN                  0

/*连接失败重新鉴权的失败次数*/
#define CONNECT_FAIL_REAUTH             5
/*连接失败超时时间,单位秒*/
#define CONNECT_FAIL_TIMEOUT            30

#define MQTT_TASK_STK_SIZE              256
#define MQTT_TASK_PRIO                  osPriorityBelowNormal
#define MQTT_SEND_Q_SIZE                32

/*MQTT TOPIC长度*/
#define MQTT_TOPIC_MAX_LEN              36

/* Exported types ------------------------------------------------------------*/

/*MQTT连接参数*/
typedef struct
{
    char  *MQTT_Server;
    uint16_t MQTT_Port;
    char  *MQTT_ClientID;
    char  *MQTT_UserName;
    char  *MQTT_PassWord;
} MQTT_ConnParam_t;

/*MQTT订阅回调函数类型*/
typedef void (*Arrived_t)(uint8_t *data, uint16_t len);

/*MQTT任务回调函数*/
typedef void (*MQTT_TaskPollFun)(void);

/*MQTT消息数据回调函数*/
typedef void (*MQTT_MsgDataFun)(char *topic,  uint8_t *payload, uint16_t len);

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void MQTT_Conn_Init(void);

void MQTT_SetHookFun(MQTT_TaskPollFun poll, MQTT_MsgDataFun sendfail);

BOOL MQTT_IsConnected(void);
BOOL MQTT_IsDataFlow(void);

int16_t MQTT_SendData(uint8_t *dat, uint16_t len);
int16_t MQTT_ReadData(uint8_t *dat, uint16_t len);

BOOL Publish_MQTT(char const *topic, Qos qos, uint8_t *payload, uint16_t len);      //发送数据
BOOL Subscribe_MQTT(char const *topic, Qos qos, BOOL isBroadcast, Arrived_t fun); //可用于心跳
int32_t Publish_ConnectHeart(char *buf,char* param);
#endif
