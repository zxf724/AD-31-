/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file Process.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.8.31
 * @brief 业务逻辑处理函数头文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _PROCESS_H
#define _PROCESS_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"
#include "config.h"
#include "cJSON.h"

/* Exported define -----------------------------------------------------------*/
/*定义分路开关的最大数*/
#define PATH_MAX    12
#define __HARD_VER  2

// times to responsed the error communication
#define MAX_RESPONSED_ERROR     30

/* Exported types ------------------------------------------------------------*/

/*定义设备参数结构体*/
typedef struct
{
    char server[64];
    uint16_t port;
    uint16_t MQTTalive;
    char WIFI_SSID[32];
    char WIFI_PWD[32];
    char WIFI_MAC[18];
    char WIFI_IP[16];
    char WIFI_SN[16];
    char WIFI_GW[16];
    uint8_t WIFI_DHCP;

    char LAN_MAC[18];
    char LAN_IP[16];
    char LAN_SN[16];
    char LAN_GW[16];
    uint8_t LAN_DHCP;

    char DNS[64];
    char PhoneNum[16];
    char APN[16];
    char APN_User[16];
    char APN_PWD[16];
} DeviceParam_t;

/*定义开关信息结构体*/
typedef struct
{
    uint8_t SPD_Status;         /*防雷器状态,0为损坏,1为完好*/

    struct {
        uint8_t Phase;          /*相数*/
        uint8_t Temp;           /*温度值，单位0.1摄氏度*/
        uint8_t SwitchStatus    /*开关状态，0为断，1为通*/;
        uint32_t Power          /*功率值，单位为0.1W*/;
        uint16_t Leak;          /*漏电流,单位为0.1mA*/
        uint16_t Voltage[3];    /*电压值,单位为0.1V*/
        uint16_t Current[3];    /*电流值,单位为0.1A*/
    } Main;

    struct {
        uint8_t Temp;           /*温度值，单位0.1摄氏度*/
        uint8_t SwitchStatus    /*开关状态，0为断，1为通*/;
        uint16_t Current;       /*电流值,单位为0.1A*/
        uint32_t Power          /*功率值，单位为0.1W/h*/;
    } Path[PATH_MAX];
} Breacker_Info_t;

/*定义开关定时结构体*/
typedef struct
{
    struct clock_t {
        uint8_t BeginDate[3];   /*开始日期*/
        uint8_t EndDate[3];     /*结束日期*/
        uint8_t OnTime[2];      /*定时开时间*/
        uint8_t OffTime[2];     /*定时关时间*/
    } Main;

    struct clock_t Path[PATH_MAX];
} Breacker_Timer_t;

/*定义开关的最大负载*/
typedef struct
{
    struct {
        uint32_t Power;
        uint16_t Current[3];
    } Main;

    uint16_t PathCurrent[PATH_MAX];
} Breacker_LoadOver_t;

/*定义电量信息结构体*/
typedef struct
{
    /*电量值，单位为W/h*/
    struct meter_t {
        uint32_t month;
        uint32_t last;
        uint32_t year;
    } Main;

    struct meter_t Path[PATH_MAX];
    int i;
} PowerMeter_t;

/*定义开关参数结构体*/
typedef struct
{
    uint8_t PathNum;                /*分路开关的数量*/
    uint32_t MainAddr;              /*总开关的地址*/
    uint32_t PathAddr[PATH_MAX];    /*分路开关的地址序列*/
    uint8_t  MainEvent;             /*总开关的事件*/
    uint8_t  PathEnent[PATH_MAX];   /*分路开关的事件*/
    Breacker_Info_t Info;
    Breacker_Timer_t Timer;
    Breacker_LoadOver_t LoadOver;
    PowerMeter_t Meter;
} BreackerParam_t;
typedef struct tagTIMING_S{
    uint8_t num;    // 1:单次��?2周循��?
    uint8_t mode;    // 1:单次��?2周循��?
    uint8_t week[8];    //第一个字节存数量
    uint8_t aucDate[4];     //
    uint8_t aucTime[4];     //
    uint8_t close;
    //uint8_t startDate[3];     //
    uint8_t startTime[3];     //
    //uint8_t endDate[3];     //
    uint8_t endTime[3];     //

    uint32_t startTick;
    uint32_t endTick;
}TIMING_S;

typedef struct tagSWITCHTIMESET_S
{
    TIMING_S timingData[5];
    //uint8_t aucBegDate[5][3];    // ������
    //uint8_t aucEndDate[5][3];
    //uint8_t aucOnTime[5][3];     // ʱ��
    //uint8_t aucOffTime[5][3];
}SWITCHTIMESET_S;

#define SWTIME_BUFSIZE_MAX  (sizeof(SWITCHTIMESET_S) * PMET_CHANNEL_CNT)
#define SWTIME_ADDR_START   SECTOR_ADDR(SECTOR_SWTIME_START)


/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/
extern lstring g_szClientId;
extern lstring g_szDomain;
extern uint16 g_usPort;

/* Exported functions --------------------------------------------------------*/
void Process_Init(void);

void Publish_DeviceParam(void);
void Publish_BreackerInfo(char* topic);
void Publish_PowerMeter(void);
uint16_t Publish_Event(uint8_t address, 
                       uint32_t sn,
                       uint16_t newevent, 
                       
                       uint16_t* poldevent, 
                       uint8_t switchstatus);
void Publish_Timer(void);
void Publish_LoadOver(void);

void Publish_Version(void);
void Publish_DFUReq(uint32_t offset, uint16_t size);

void PublishSParam(uint8 aucBuf[8]);
void Publish_Reset(void);
void Publish_BreackerSort(char* topic);
void ControlToCommunicationPoll(uint8_t *dat);

void CommunicationToControlPoll(uint8_t byte2 , uint8_t byte3);
void VailResponse(uint8_t dat0);
BOOL CMD_Updata(char *cmd, cJSON *desired);


#endif
