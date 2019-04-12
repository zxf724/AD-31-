/**
 * **********************************************************************
 *             Copyright (c) 2016 AFU. All Rights Reserved.
 * @file wifi.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.7.14
 * @brief WIFI模块驱动及管理函数头文件,用于ESP8266.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _WIFI_H
#define _WIFI_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"
#include "system.h"
/* Exported define -----------------------------------------------------------*/
#define NET_WIFI_EN                 1

/*WIFI的串口号*/
#define WIFI_UART_PORT      3

#define WIFI_TASK_STK_SIZE          256
#define WIFI_TASK_PRIO              osPriorityNormal
#define WIFI_SEND_Q_SIZE            8

#define WIFI_UART_REFRESH_TICK      100

#define WIFI_AT_ECHO_DEFAULT        TRUE

/*定义超时时间,单位秒*/
/*配网超时退出的时间*/
#define WIFI_ELINK_TIMEOUT          120

/*路由未连接*/
#define WIFI_STA_TIMOUT             15
/*TCP未连接*/
#define WIFI_NOT_CONNECT_TIMEOUT    300

#define WIFI_SEND_MAX_SIZE          1800
#define WIFI_RECEIVE_MAX_SIZE       2048

/*WIFI远程调试使能*/
#define WIFI_CMD_EN                 0

/*WIFI省电使能，使能后当WIFI长时间未连网时关机*/
#define WIFI_POWER_SAVE_EN          1

/*WIFI省电关机的时间，单位秒*/
#define WIFI_POWER_SAVE_TIME        60

/*定义WIFI模块控制选项*/
#define WIFI_OPT_RESET              0x01
#define WIFI_OPT_SET_STATION        0x04
#define WIFI_OPT_SET_SOCKET         0x08
#define WIFI_OPT_SET_IPCONFIG       0x10

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    wifi_status_poweroff = 0,
    wifi_status_poweron,
    wifi_status_elink,
    wifi_status_station_notconnected,
    wifi_status_station_connected,
    wifi_status_socket_connected,
    wifi_status_fault,
} WIFI_Status_t;


/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void WIFI_Init(void);

void WIFI_ReStart(void);
void WIFI_SetOnOff(BOOL onoff);
WIFI_Status_t WIFI_ReadStatus(void);

int16_t WIFI_SocketSendData(uint8_t *data, uint16_t len);
int8_t WIFI_IsSocketConnect(void);
void WIFI_SetSocketParam(char *server, uint16_t port, Sock_RecCBFun callback);
uint8_t WIFI_ReadRSSI(void);
void WIFI_SetStation(char *ssid, char *pwd);

#endif
