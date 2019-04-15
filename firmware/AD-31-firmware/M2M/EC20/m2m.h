/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file M2M.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.4.1
 * @brief M2M驱动函数头文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _M2M_H
#define _M2M_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"

/* Exported define -----------------------------------------------------------*/
#define M2M_TASK_STK_SIZE          128
#define M2M_TASK_PRIO              osPriorityNormal
#define M2M_SEND_Q_SIZE            8

#define M2M_UART_BDR               115200
#define M2M_UART_REFRESH_TICK      1000

#define M2M_SEND_MAX_SIZE          520
#define M2M_RECEIVE_MAX_SIZE       520

/*M2M省电使能，使能M2M4G长时间未连网时关机*/
#define M2M_POWER_SAVE_EN          0

/*M2M省电关机的时间，单位秒*/
#define M2M_POWER_SAVE_TIME        60

/*定义M2M模块控制选项*/
#define M2M_OPT_RESET              0x01
#define M2M_OPT_SET_SOCKET         0x04
#define M2M_OPT_SET_APN            0x08

/* Exported types ------------------------------------------------------------*/
typedef enum {
    M2M_status_poweroff = 0,
    M2M_status_poweron,
    M2M_status_nocard,
    M2M_status_nonet,
    M2M_status_online,
    M2M_status_fault
} M2M_Status_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void M2M_Init(void);

void M2M_ReStart(void);
void M2M_SetOnOff(BOOL onoff);
M2M_Status_t M2M_ReadStatus(void);

int16_t M2M_SocketSendData(uint8_t *data, uint16_t len);
int8_t M2M_IsSocketConnect(void);
void M2M_SetSocketParam(char *server, uint16_t port, Sock_RecCBFun callback);

uint8_t M2M_ReadRSSI(void);
BOOL    M2M_ReadPhoneNum(char *num);

BOOL M2M_TTS(char *text);

#endif


