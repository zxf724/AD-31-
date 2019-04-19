/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file DataSave.c
 * @version V1.0
 * @date 2016.12.16
 * @brief 数据存储处理函数.
 *
 * *********************************************************************
 * @note
 *
 * *********************************************************************
 * @author 宋阳
 */



/* Includes ------------------------------------------------------------------*/
#include "datasave.h"
#include "spi_flash.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
WorkParam_t  WorkParam;

/* Private function prototypes -----------------------------------------------*/
static void datasave_Console(int argc, char *argv[]);

/* Exported functions --------------------------------------------------------*/
/**
 * 业务处理初始
 */
void DataSave_Init(void)
{
    CMD_ENT_DEF(datasave, datasave_Console);
    Cmd_AddEntrance(CMD_ENT(datasave));
    DBG_LOG("DataSave Init.");
}

/**
 * 工作参数初始化，如存储数据有错或为空，则恢复默认参数.
 */
void WorkParam_Init(void)
{
    uint16_t crc = 0;
    uint32_t addr = 0;

    addr = SECTOR_ADDR(WORK_PARAM_SECTOR);
    SFlash_Read(addr, (uint8_t *)&WorkParam, OBJ_LEN(WorkParam_t));
    
    crc = CRC_16(0, (uint8_t *)&(WorkParam)+4, OBJ_LEN(WorkParam_t) - 4);
    if (WorkParam.crc == crc) {        
        DBG_LOG("Work parameter sector1 load OK.");
        return;
    } else {
        addr = SECTOR_ADDR(WORK_PARAM_SECTOR + 1);
        SFlash_Read(addr, (uint8_t *)&WorkParam, OBJ_LEN(WorkParam_t));

        crc = CRC_16(0, (uint8_t *)&(WorkParam)+4, OBJ_LEN(WorkParam_t) - 4);
        if(WorkParam.mqtt.MQTT_ClientID[0] == '\0' || WorkParam.mqtt.MQTT_Server[0] == '\0' || WorkParam.mqtt.MQTT_Port == 0){
            crc = 0;
        }
    }
    
    if (WorkParam.crc == crc) {
        DBG_LOG("Work parameter sector2 load OK.");
        return;
    }
    if (WorkParam.version == BIT16_MAX || WorkParam.version == 0) {
        addr = SECTOR_ADDR(WORK_PARAM_SECTOR);
        SFlash_Read(addr, (uint8_t *)&WorkParam, OBJ_LEN(WorkParam_t));
        DBG_LOG("NO Work parameter!!!");
        WorkParam.version = 0;
    } else {
        DBG_LOG("Work parameter Break!!!");
    }
    DBG_LOG("Work parameter set default.");

    if (WorkParam.version < 1) {
        WorkParam.version = 1;
        /*初始化mqtt参数*/
        strcpy (WorkParam.mqtt.MQTT_ClientID, "AD13-1812250001");
        #if MQTT_TYPE == 0
        strcpy (WorkParam.mqtt.MQTT_Server, "47.106.178.36");//47.106.178.36
        //WorkParam.mqtt.MQTT_Server[0] = 0;
        WorkParam.mqtt.MQTT_Port = 61613;
        #elif MQTT_TYPE == 1
        /*dianxiang.sztosee.cn ,8306//jiduo.sztosee.cn,8513strcpy (WorkParam.mqtt.MQTT_Server, "47.106.212.243");
        //WorkParam.mqtt.MQTT_Server[0] = 0;
        WorkParam.mqtt.MQTT_Port = 61613;
        strcpy(WorkParam.mqtt.MQTT_UserName, "admin");
        strcpy(WorkParam.mqtt.MQTT_PassWord, "123456");*/
        strcpy (WorkParam.mqtt.MQTT_Server, "dianxiang.sztosee.cn");
        //WorkParam.mqtt.MQTT_Server[0] = 0;
        WorkParam.mqtt.MQTT_Port = 8306;
        #endif
        strcpy(WorkParam.mqtt.MQTT_UserName, "admin");
        strcpy(WorkParam.mqtt.MQTT_PassWord, "password");
        WorkParam.mqtt.MQTT_Timout = 10000;
        WorkParam.mqtt.MQTT_PingInvt = 50;
        WorkParam.mqtt.MQTT_DataInvt = 120;
        WorkParam.mqtt.MQTT_DataEnable = TRUE;
        /*初始化M2M参数*/
        WorkParam.M2M.APN[0] = 0;
        WorkParam.M2M.APN_User[0] = 0;
        WorkParam.M2M.APN_PWD[0] = 0;

        /*初始化启动日志记*/
        WorkParam.StartLogAddr = 0;

        DBG_LOG("Work parameter verison1 default;%s,%s,%d.",WorkParam.mqtt.MQTT_ClientID,WorkParam.mqtt.MQTT_Server,WorkParam.mqtt.MQTT_Port );
    }
    WorkParam_Save();
    
}

/**
 * 工作参数存储
 * @return
 */
BOOL WorkParam_Save(void)
{
    uint16_t crc = 0;
    uint32_t addr = 0, len = OBJ_LEN(WorkParam_t);
    crc = CRC_16(0, (uint8_t *)&(WorkParam)+4, len - 4);
    WorkParam.crc = crc;
    addr = SECTOR_ADDR(WORK_PARAM_SECTOR);
    SFlash_EraseSectors_NotCheck(addr, 1);
    SFlash_Write_NotCheck(addr, (uint8_t *)&WorkParam, len);
    addr = SECTOR_ADDR(WORK_PARAM_SECTOR + 1);
    SFlash_EraseSectors_NotCheck(addr, 1);
    return SFlash_Write_NotCheck(addr, (uint8_t *)&WorkParam, len);
}

/* Private function prototypes -----------------------------------------------*/

/**
 * 数据存储调试命令
 * @param argc 参数项数
 * @param argv 参数列表
 */
static void datasave_Console(int argc, char *argv[])
{

    argv++;
    argc--;
    if (ARGV_EQUAL("workparam")) {
        DBG_LOG("WorkParam length:%u", OBJ_LEN(WorkParam));
    }
    else if (ARGV_EQUAL("netfactoryreset")) {
        uint16_t d = 0;
        d = uatoi(argv[1]);
        DBG_LOG("netfactoryreset %dms latter.", d);
        osDelay(d + 5);
        WorkParam.version = 0;
        WorkParam_Init();
        CMD_Virtual("system reset 5000");
    }
    else if (ARGV_EQUAL("setid")) {
        strcpy(WorkParam.mqtt.MQTT_ClientID, (argv[1]));        
        WorkParam_Save();
        DBG_LOG("setid ok :%s.", WorkParam.mqtt.MQTT_ClientID);
    }
    else if (ARGV_EQUAL("setservice")) {
        DBG_LOG("argv0:%s,%s,%s.", argv[0],argv[1]);
        strcpy(WorkParam.mqtt.MQTT_Server, (argv[1]));        
        WorkParam_Save();
        DBG_LOG("setservice ok :s.", WorkParam.mqtt.MQTT_Server);
    }
    else if (ARGV_EQUAL("setport")) {
        WorkParam.mqtt.MQTT_Port = uatoi(argv[1]);       
        WorkParam_Save();
        DBG_LOG("setport ok :s.", WorkParam.mqtt.MQTT_Port);
    }
}
