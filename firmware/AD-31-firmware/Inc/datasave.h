/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file Datasave.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.12.15
 * @brief 业务逻辑处理函数头文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _DATASAVE_H
#define _DATASAVE_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"

/* Exported define -----------------------------------------------------------*/
/*定义一个存储块的长度，需能被flash的扇区大小整除*/
#define DATA_REPT_SAVE_BLOCK_SIZE   256

#define LOG_SAVE_BLOCK_SIZE         64

/* Exported types ------------------------------------------------------------*/
/*MQTT连接参数*/
typedef struct
{
    char  MQTT_Server[32];
    char  MQTT_ClientID[48];
    char  MQTT_UserName[20];
    char  MQTT_PassWord[20];
    uint16_t MQTT_Port;
    uint16_t MQTT_Timout;
    uint16_t MQTT_PingInvt;
    uint16_t MQTT_DataInvt;
    BOOL  MQTT_DataEnable;
} MQTT_Param_t;

/*M2M参数*/
typedef struct
{
    char APN[16];
    char APN_User[16];
    char APN_PWD[16];
} M2M_WorkParam_t;

/*工作参数*/
typedef struct
{
    uint16_t version;
    uint16_t crc;
    MQTT_Param_t mqtt;
    M2M_WorkParam_t M2M;
    uint32_t StartLogAddr;
} WorkParam_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define PARAM_LEGAL(par)    (isalnum(par[0]) && (strlen(par) < OBJ_LEN(par)))

#define SAVE_ADDR()         (SECTOR_ADDR(DATA_SAVE_SECTOR))
#define SAVE_ADDR_MAX()     (SECTOR_ADDR(DATA_SAVE_SECTOR + SECTOR_DATA_SAVE_SIZE))

/* Exported variables --------------------------------------------------------*/
extern WorkParam_t          WorkParam;

/* Exported functions --------------------------------------------------------*/
void DataSave_Init(void);

void WorkParam_Init(void);
BOOL WorkParam_Save(void);

#endif
