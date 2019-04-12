/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file Process.c
 * @version V1.0
 * @date 2016.8.31
 * @brief 业务逻辑处理函数.
 *
 * *********************************************************************
 * @note
 *
 * *********************************************************************
 * @author 宋阳
 */



/* Includes ------------------------------------------------------------------*/
#include "user_comm.h"
#include "config.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
//static DeviceParam_t DeviceParam;
//static BreackerParam_t Param;

lstring g_szClientId = "ID002";
lstring g_szDomain = "www.sztosee.cn"; //
uint16 g_usPort = 1883;

/* Private function prototypes -----------------------------------------------*/
static BOOL PublishPath(char *topic, cJSON *Body);
static  void remote_data_arriva(uint8_t *dat, uint16_t len);
static void process_Console(int argc, char *argv[]);
static BOOL CMD_Updata(char *cmd, cJSON *desired);
static void TimeSwitchAdd(cJSON* timejson, uint8_t i);

typedef void (*calltimeswitchadd)(cJSON* timejson, uint8_t chnn);

static void AllInfo_Updata(calltimeswitchadd timeswitch);
/* Exported functions --------------------------------------------------------*/

/**
 * 业务处理初始
 */
void Process_Init(void)
{
    PARAM_Init();
    PMET_Init();
    CMD_ENT_DEF(process, process_Console);
    Cmd_AddEntrance(CMD_ENT(process));
    //添加MQTT连接的参数即
    //strcpy (WorkParam.mqtt.MQTT_Server, "61.146.113.106");//"101.231.241.28");
    //strcpy(WorkParam.mqtt.MQTT_ClientID, g_szClientId);
    //WorkParam.mqtt.MQTT_Port = 1883;//g_usPort;
    Subscribe_MQTT("event/raw", QOS1, FALSE, remote_data_arriva);
    DBG_LOG("Process Start.");
}
static void net_param_updata(void){
    cJSON *desired = NULL;
    desired = cJSON_CreateObject();
    if (desired != NULL) {
        cJSON_AddNumberToObject(desired, "timestamp", RTC_ReadTick() - 28800);
        cJSON_AddStringToObject(desired, "ip", WorkParam.mqtt.MQTT_Server);
        cJSON_AddNumberToObject(desired, "port", WorkParam.mqtt.MQTT_Port);
        cJSON_AddNumberToObject(desired, "heartbeat", WorkParam.mqtt.MQTT_PingInvt);
        cJSON_AddStringToObject(desired, "project", PROJECT);
        cJSON_AddStringToObject(desired, "firmware", VERSION);
        cJSON_AddStringToObject(desired, "hardware", VERSION_HARDWARE);
        cJSON_AddStringToObject(desired, "status", "ok");
        CMD_Updata("CMD-101", desired);
    }
}

void AllInfo_Updata(calltimeswitchadd timeswitch){
    cJSON *desired = NULL,*array = NULL,*arraydata = NULL,*devicedata = NULL,*status = NULL;
    cJSON *monthcount = NULL, *cruarray = NULL;
    double fTmp = 0;
    uint32 *paulelecinfo = NULL;
    PMET_CHANNELINFO_S *pstchninfo;
    uint8_t i = 1;
    while (i < PMET_CHANNEL_CNT){       
        pstchninfo = PMET_GetChnInfo(i);
        if (NULL != pstchninfo) {
            desired = cJSON_CreateObject();
            if (desired != NULL) {
                cJSON_AddItemToObject(desired, "arrays",array = cJSON_CreateArray());
                cJSON_AddItemToArray(array, arraydata = cJSON_CreateObject());
                cJSON_AddItemToObject(arraydata, "device",devicedata = cJSON_CreateObject());
                cJSON_AddNumberToObject(devicedata, "index", i);
                if(pstchninfo->type == 1){
                    cJSON_AddStringToObject(devicedata, "mode", "C63"); 
                }
                else if(pstchninfo->type == 2){
                    cJSON_AddStringToObject(devicedata, "mode", "C32");
                }
                //uint32_t id = 1712240001 + (uatoix(WorkParam.mqtt.MQTT_ClientID + 6)* 100);
                cJSON_AddNumberToObject(devicedata, "id", pstchninfo->sn);
                cJSON_AddItemToObject(arraydata, "status",status = cJSON_CreateObject());            
                cJSON_AddNumberToObject(status, "event", 0);
                fTmp = pstchninfo->powers;
                cJSON_AddNumberToObject(status, "power",  fTmp);
                fTmp = pstchninfo->limitpower ;
                cJSON_AddNumberToObject(status, "loadmax", fTmp);
                cJSON_AddNumberToObject(status, "temp", pstchninfo->temperature);
                cJSON_AddNumberToObject(status, "tempmax", pstchninfo->limittemp);
                paulelecinfo = PMET_GetElecInfoAddr(i);         
                DBG_LOG("[PMET] pulElecInfom[0]:%d,pulElecInfom[1]:%d,pstMainChnInfo->ulElectricity:%d\n", 
                *(paulelecinfo), *(paulelecinfo + 1),pstchninfo->energyp);
                fTmp = pstchninfo->energyp - paulelecinfo[0];
                fTmp = fTmp / 10.0;
                //fTmp = pstMainChnInfo->ulElectricity - paulelecinfo[0];
                //fTmp = fTmp / 1000.0;
                cJSON_AddNumberToObject(status, "meterd", fTmp);
                fTmp = pstchninfo->energyp - paulelecinfo[2];
                fTmp = fTmp / 10.0;
                cJSON_AddNumberToObject(status, "meterm", fTmp);
                cJSON_AddNumberToObject(status, "switch", pstchninfo->switchstatus);   
                cJSON_AddNumberToObject(status, "auto", pstchninfo->switchstatus);            
                cJSON_AddFalseToObject(status, "auto");
                cJSON_AddNumberToObject(status, "leakage", pstchninfo->leakage);
                cJSON_AddItemToObject(status, "current",cruarray = cJSON_CreateArray());
                cJSON_AddItemToArray(cruarray, cJSON_CreateNumber(pstchninfo->current[0]));
                cJSON_AddItemToObject(status, "voltage",cruarray = cJSON_CreateArray());
                cJSON_AddItemToArray(cruarray, cJSON_CreateNumber(pstchninfo->voltage[0]));
                cJSON_AddItemToObject(arraydata, "meter",monthcount = cJSON_CreateArray());
                cJSON_AddItemToArray(monthcount, cruarray = cJSON_CreateObject());
                timeRTC_t time_;
                RTC_ReadTime(&time_);
                uint8_t month = 0;
                if(time_.month == 1){
                    month = 12;
                }
                else{
                    month = time_.month - 1;
                }
                cJSON_AddNumberToObject(cruarray, "month", month);
                fTmp = paulelecinfo[2] - paulelecinfo[3];
                fTmp = fTmp / 10.0;
                cJSON_AddNumberToObject(cruarray, "value", fTmp);
                if(timeswitch != NULL){
                    timeswitch(arraydata, 1);
                }
                CMD_Updata("CMD-104", desired);

            }
        }
        i++;
    }
}
void switch_count_pack(cJSON* arraydata, uint8_t channel){
    cJSON  *monthcount = NULL, *powercjson;
    uint32 *paulelecinfo = NULL;
    uint32_t temp = 0;
    paulelecinfo = PMET_GetElecInfoAddr(channel); 
    if(paulelecinfo != NULL){
        cJSON_AddItemToObject(arraydata, "meter",monthcount = cJSON_CreateArray());
        cJSON_AddItemToArray(monthcount, powercjson = cJSON_CreateObject());
        timeRTC_t time_;
        RTC_ReadTime(&time_);
        uint8_t month = 0;
        if(time_.month == 1){
            month = 12;
        }
        else{
            month = time_.month - 1;
        }
        cJSON_AddNumberToObject(powercjson, "month", month);
        temp = paulelecinfo[2] - paulelecinfo[3];
        cJSON_AddNumberToObject(powercjson, "value", temp);
    }
   
}
/**
 * @brief  统计信息上传
 * @note   
 * @retval None
 */
void switch_count_updata(void)
{
    cJSON *desired = NULL,*array = NULL,*arraydata = NULL,*devicedata = NULL;
    PMET_CHANNELINFO_S *pstchninfo;    
    uint8_t i = 1;
    //uint8 ucEndChn = 0;
    while (i < PMET_CHANNEL_CNT){
        pstchninfo = PMET_GetChnInfo(i);
        if (NULL != pstchninfo) {
            desired = cJSON_CreateObject();
            if (desired != NULL) {
                cJSON_AddItemToObject(desired, "arrays",array = cJSON_CreateArray());
                cJSON_AddItemToArray(array, arraydata = cJSON_CreateObject());
                cJSON_AddItemToObject(arraydata, "device",devicedata = cJSON_CreateObject());
                cJSON_AddNumberToObject(devicedata, "inidex", i);
                if(pstchninfo->type == 1){
                    cJSON_AddStringToObject(devicedata, "mode", "C63"); 
                }
                else if(pstchninfo->type == 2){
                    cJSON_AddStringToObject(devicedata, "mode", "C32");
                }
                cJSON_AddNumberToObject(devicedata, "id", pstchninfo->sn);
                switch_count_pack(arraydata, i);                
                CMD_Updata("CMD-106", desired);
            }
        }
        i++;
    }
}
void TimerInfo_Updata(void)
{
    cJSON *desired = NULL,*array = NULL,*arraydata = NULL,*devicedata = NULL;
    PMET_CHANNELINFO_S *pstchninfo;    
    uint8_t i = 1;
    //uint8 ucEndChn = 0;
    while (i < PMET_CHANNEL_CNT){
        pstchninfo = PMET_GetChnInfo(i);
        if (NULL != pstchninfo) {
            desired = cJSON_CreateObject();
            if (desired != NULL) {
                cJSON_AddItemToObject(desired, "arrays",array = cJSON_CreateArray());
                cJSON_AddItemToArray(array, arraydata = cJSON_CreateObject());
                cJSON_AddItemToObject(arraydata, "device",devicedata = cJSON_CreateObject());
                cJSON_AddNumberToObject(devicedata, "inidex", i);
                if(pstchninfo->type == 1){
                    cJSON_AddStringToObject(devicedata, "mode", "C63"); 
                }
                else if(pstchninfo->type == 2){
                    cJSON_AddStringToObject(devicedata, "mode", "C32");
                }
                cJSON_AddNumberToObject(devicedata, "id", pstchninfo->sn);
                TimeSwitchAdd(arraydata, i);
                CMD_Updata("CMD-107", desired);
            }
        }
        i++;
    }
}

static void Switch_ParamSet(cJSON * topic, uint8_t chnn)
{
    cJSON * loaddata = NULL;
    loaddata = cJSON_GetObjectItem(topic, "loadmax");//
    if(loaddata != NULL && loaddata->type == cJSON_Number){
        //loaddata->valueint;
        PMET_SetChnParam(chnn, 0xffffffff, (uint32)(loaddata->valueint), PMET_CMD_SET_POWER);
        DBG_LOG("POWR set:%d", (uint32)(loaddata->valueint));
    }
    loaddata = cJSON_GetObjectItem(topic, "tempmax");//
    if(loaddata != NULL && loaddata->type == cJSON_Number){
        //loaddata->valueint;PMET_CMD_SET_TEMPE
        PMET_SetChnParam(chnn, 0xffffffff, (uint32)(loaddata->valueint),PMET_CMD_SET_TEMPE);
    }
    loaddata = cJSON_GetObjectItem(topic, "switch");//
    if(loaddata != NULL ){
        if(loaddata->type == cJSON_Number){
            if(loaddata->valueint == 1){
                PMET_SetChannelSwitch(chnn, 0xffffffff, 1, PMET_CMD_SET_SWITCH);
            }
            else{
                PMET_SetChannelSwitch(chnn, 0xffffffff, 0, PMET_CMD_SET_SWITCH);
            }
        }
    }
    loaddata = cJSON_GetObjectItem(topic, "test");//
    if(loaddata != NULL ){
        if(loaddata->type == cJSON_True){
            PMET_SetChnParam(chnn, 0xffffffff, (uint32)(loaddata->valueint),PMET_CMD_LEAKAGE_TEST);
        }        
    }
    loaddata = cJSON_GetObjectItem(topic, "auto");//
    if(loaddata != NULL ){
        if(loaddata->type == cJSON_True){
            PMET_SetChannelSwitch(chnn, 0xffffffff, 1, PMET_CMD_AUTO_ALLOW);
        }
        else{
            PMET_SetChannelSwitch(chnn, 0xffffffff, 0, PMET_CMD_AUTO_ALLOW);
        }
    }
    
}
/**
 * [TimeSwitchAdd description]
 * @param timejson [description]
 */

static void TimeSwitchAdd(cJSON* timejson, uint8_t chnn){
    cJSON* timeswitch = NULL;
    cJSON* swtich_mem = NULL;
    static SWITCHTIMESET_S *pstSwitchTime;
    pstSwitchTime = pvPortMalloc(SWTIME_BUFSIZE_MAX);
    if(pstSwitchTime == NULL){
        return;
    }
    FLASH_Read(SWTIME_ADDR_START, (uint8 *)pstSwitchTime, SWTIME_BUFSIZE_MAX);
    timeswitch = cJSON_CreateArray();
    uint8_t flag = 0;
    for(uint8_t time_i = 0; time_i < 5; time_i++){
        if(pstSwitchTime[chnn - 1].timingData[time_i].num < 5){
            flag = 1;
            cJSON_AddItemToArray(timeswitch, swtich_mem = cJSON_CreateObject());
            cJSON_AddNumberToObject(swtich_mem, "num", time_i);
            cJSON_AddNumberToObject(swtich_mem, "mode", pstSwitchTime[chnn - 1].timingData[time_i].mode);
            if(pstSwitchTime[chnn].timingData[time_i].mode != 0){
                cJSON_AddNumberToObject(swtich_mem, "start", pstSwitchTime[chnn - 1].timingData[time_i].startTick);
                cJSON_AddNumberToObject(swtich_mem, "end", pstSwitchTime[chnn - 1].timingData[time_i].endTick);
                char temp[10] = {0};
                memset(temp, 0x00, 10);
                sprintf(temp, "%02x.%02x", pstSwitchTime[chnn - 1].timingData[time_i].startTime[0],
                    pstSwitchTime[chnn - 1].timingData[time_i].startTime[1]);
                cJSON_AddStringToObject(swtich_mem, "on", temp);
                memset(temp, 0x00, 10);
                sprintf(temp, "%02x.%02x", pstSwitchTime[chnn - 1].timingData[time_i].endTime[0],
                        pstSwitchTime[chnn - 1].timingData[time_i].endTime[1]);
                cJSON_AddStringToObject(swtich_mem, "off", temp);
                cJSON *cycle = NULL;
                cJSON_AddItemToObject(swtich_mem, "cycle",cycle = cJSON_CreateArray());

                for(uint8_t weeki = 0; weeki < 7; weeki++){
                    if(pstSwitchTime[chnn - 1].timingData[time_i].week[weeki] == FALSE){
                        cJSON_AddItemToArray(cycle, cJSON_CreateFalse());
                    }
                    else{
                        cJSON_AddItemToArray(cycle, cJSON_CreateTrue());
                    }
                }
            }
        }
    }
    if(flag == 1){
        cJSON_AddItemToObject(timejson, "timer",timeswitch);
    }
    else{
        cJSON_Delete(timeswitch);
    }
    vPortFree(pstSwitchTime);
}

static void Switch_TimeSet(cJSON * topic, uint8_t chnn){
    cJSON * timeSet = NULL;
    SWITCHTIMESET_S *pstSwitchTime;
    pstSwitchTime = pvPortMalloc(SWTIME_BUFSIZE_MAX);
     if (NULL == pstSwitchTime) {
        return;
    }
    FLASH_Read(SWTIME_ADDR_START, (uint8 *)pstSwitchTime, SWTIME_BUFSIZE_MAX);
   
    uint8_t num = 0;
    uint8_t iarray = cJSON_GetArraySize(topic);
    for(uint8_t i = 0; i < iarray; i++){
        cJSON * timenum = cJSON_GetArrayItem(topic, i);
        if(timenum != NULL){
            timeSet = cJSON_GetObjectItem(timenum, "num");
            if(timeSet && timeSet->type == cJSON_Number){
                num = timeSet->valueint;
                pstSwitchTime[chnn - 1].timingData[num].num = timeSet->valueint;
            }
            timeSet = cJSON_GetObjectItem(timenum, "mode");//
            if(timeSet && timeSet->type == cJSON_Number){
                pstSwitchTime[chnn - 1].timingData[num].mode = timeSet->valueint;
            }
            timeSet = cJSON_GetObjectItem(timenum, "start");//
            if(timeSet && timeSet->type == cJSON_Number){
                pstSwitchTime[chnn - 1].timingData[num].startTick = timeSet->valueint;
            }
            timeSet = cJSON_GetObjectItem(timenum, "end");//
            if(timeSet && timeSet->type == cJSON_Number){
                pstSwitchTime[chnn - 1].timingData[num].endTick = timeSet->valueint;
            }
            timeSet = cJSON_GetObjectItem(timenum, "on");//
            if(timeSet != NULL && timeSet->type == cJSON_String){
                //pstSwitchTime[chnn].timingData[0].endTick = timeSet->valueint;
                sscanf(timeSet->valuestring, "%02x.%02x", &pstSwitchTime[chnn - 1].timingData[num].startTime[0],
                    &pstSwitchTime[chnn - 1].timingData[num].startTime[1]);
            }
            timeSet = cJSON_GetObjectItem(timenum, "off");//
            if(timeSet != NULL && timeSet->type == cJSON_String){
                sscanf(timeSet->valuestring, "%02x.%02x", &pstSwitchTime[chnn- 1].timingData[num].endTime[0],
                    &pstSwitchTime[chnn - 1].timingData[num].endTime[1]);
            }
            timeSet = cJSON_GetObjectItem(timenum, "cycle");//
            if(timeSet != NULL && timeSet->type == cJSON_Array){
                uint8_t iSize = cJSON_GetArraySize(timeSet);
                //pstSwitchTime[chnn].timingData[0].week[0] = iSize;
                for(uint8_t weeki = 0; weeki < iSize; weeki++){
                    cJSON * week = cJSON_GetArrayItem(timeSet, weeki);
                    if(week != NULL){
                        pstSwitchTime[chnn - 1].timingData[num].week[weeki] = week->type;
                    }
                }
            }
        }
    }
    FLASH_Erase(SWTIME_ADDR_START);
    FLASH_OnlyWrite(SWTIME_ADDR_START, (uint8 *)pstSwitchTime, SWTIME_BUFSIZE_MAX);
    vPortFree(pstSwitchTime);
}
/**
 * 
 */
void Publish_DeviceParam(void)
{

}

/**
 * [Publish_Reset]
 * @method Publish_Reset
 * date
 * datetime
 */
void Publish_Reset(void)
{
    cJSON *root;
    char time[32];
    root = cJSON_CreateObject();
    if (root != NULL) {
        RTC_ReadTimeStr(time);
        //cJSON_AddStringToObject(root, "id",       WorkParam.mqtt.MQTT_ClientID);
        cJSON_AddStringToObject(root, "time",     time);
        PublishPath("Reset", root);
    }
}




/**
 * 
 */
uint16_t Publish_Event(uint8_t address, 
                       uint32_t sn,
                       uint16_t newevent, 
                       uint16_t* poldevent, 
                       uint8_t switchstatus){
    uint16_t change = 0;  
    uint8_t event = 0;
    change = newevent ^ (*poldevent);    
    if(change == 0) return 0;
    
    for(uint8_t i = 1; i < 17; i++){
        if(((newevent & change) >> i) == 1){
            cJSON * desired,*array,*arraydata ,*devicedata,*status;    
            desired = cJSON_CreateObject();
            if (desired != NULL) {
                cJSON_AddItemToObject(desired, "arrays",array = cJSON_CreateArray());
                cJSON_AddItemToArray(array, arraydata = cJSON_CreateObject());
                cJSON_AddItemToObject(arraydata, "device",devicedata = cJSON_CreateObject());
                cJSON_AddNumberToObject(devicedata, "index", address);
                //cJSON_AddStringToObject(devicedata, "mode", "SSDZ-63-C32");
                cJSON_AddNumberToObject(devicedata, "id", sn);
                cJSON_AddItemToObject(arraydata, "status",status = cJSON_CreateObject());
                if(switchstatus == 0){
                    event = 8;
                }
                else{
                    event = 9;
                }
                cJSON_AddNumberToObject(status, "event", event);
                cJSON_AddNumberToObject(status, "switch", switchstatus);
                CMD_Updata("CMD-104", desired);
            }
        }
    }  
    *poldevent = newevent;
    return change;  
}

/**
 * 
 */
void Publish_Timer(void)
{


}



/**
 * 
 */
void Publish_Con(void)
{
    cJSON *root;
    char time[32];
    root = cJSON_CreateObject();
    if (root != NULL) {
        RTC_ReadTimeStr(time);
        //cJSON_AddStringToObject(root, "ID",       WorkParam.mqtt.MQTT_ClientID);
        cJSON_AddStringToObject(root, "time", time);
        //cJSON_AddItemToObjectCS(root, param, bodyMain = cJSON_CreateObject());

        PublishPath("Connect", root);
    }
}
void Publish_Version(void)
{
    cJSON * root,*body;

    root = cJSON_CreateObject();
    if (root != NULL) {
        //cJSON_AddStringToObject(root, "ID",       WorkParam.mqtt.MQTT_ClientID);
        cJSON_AddItemToObjectCS(root, "MCU", body = cJSON_CreateObject());
        cJSON_AddStringToObject(body, "version", VERSION);

        PublishPath("FWVersion", root);
    }
}

/**
 * 
 */
void Publish_DFUReq(uint32_t offset, uint16_t size)
{
    cJSON * root,*body;

    root = cJSON_CreateObject();
    if (root != NULL) {
        cJSON_AddStringToObject(root, "id",       WorkParam.mqtt.MQTT_ClientID);
        cJSON_AddItemToObjectCS(root, "MCU", body = cJSON_CreateObject());
        cJSON_AddNumberToObject(body, "offset", offset);
        cJSON_AddNumberToObject(body, "size", size);

        PublishPath("FWDataReq", root);
    }
}





/**
 * @brief  
 * @note  基本信息上传，主要上传开关的ID 
 * @retval None
 */
static void switchinfo_updata(void){
    cJSON *desired = NULL,*array = NULL,*arraydata[32] = NULL;
    //uint32 *paulelecinfo = NULL;
    PMET_CHANNELINFO_S *pstchninfo;
    uint8_t i = 1;
    pstchninfo = PMET_GetChnInfo(i);
    if(pstchninfo != NULL){
        desired = cJSON_CreateObject();
        cJSON_AddItemToObject(desired, "arrays",array = cJSON_CreateArray());
    }    
    while (i < PMET_CHANNEL_CNT){ 
        pstchninfo = PMET_GetChnInfo(i);
        if (NULL != pstchninfo) {
            if(desired == NULL){
                desired = cJSON_CreateObject();
                cJSON_AddItemToObject(desired, "arrays",array = cJSON_CreateArray());
            }
            if (desired != NULL) {
                cJSON_AddItemToArray(array, arraydata[i - 1]  = cJSON_CreateObject());
                cJSON_AddNumberToObject(arraydata[i - 1], "index", i);
                cJSON_AddStringToObject(arraydata[i - 1], "mode", "C32");
                cJSON_AddNumberToObject(arraydata[i - 1], "id", pstchninfo->sn);  
            }            
        }        
        i++;
    }
    if(desired != NULL){
        CMD_Updata("CMD-108", desired);
    }
}
static void set_switch_parem(cJSON *topic){  
    cJSON * arr = cJSON_GetObjectItem(topic, "arrays");
    if (arr && arr->type == cJSON_Array) {
        uint8_t iSize = cJSON_GetArraySize(arr);
        for(uint8_t i = 0; i < iSize; i++){
            cJSON * data = cJSON_GetArrayItem(arr, i);
            if(arr == NULL)
                break;
            cJSON * device = cJSON_GetObjectItem(data, "device");
            if(device == NULL || device->type != cJSON_Object)
                break;
            cJSON * index = cJSON_GetObjectItem(device, "index");
            if(index == NULL || index->type != cJSON_Number)
                break;
            //index = index->valueint;
            cJSON * status = cJSON_GetObjectItem(data, "status");
            if(status != NULL && status->type == cJSON_Object){
                Switch_ParamSet(status, index->valueint);
                DBG_LOG("Switch_ParamSet\r");
            }
            cJSON * timeswitch = cJSON_GetObjectItem(data, "timer");
            if(timeswitch != NULL &&  timeswitch->type == cJSON_Array){                               
                Switch_TimeSet(timeswitch, index->valueint);
            }
            
        }
    }    
}    

static void set_rtc(uint32_t rtc){
    timeRTC_t stTime;
    timeRTC_t stTimeloc;
    RTC_TickToTime(rtc + 28800, &stTime);
    RTC_ReadTime(&stTimeloc);
    if(memcmp(&stTime, &stTimeloc, sizeof(stTime)) != 0){
        if (FALSE == RTC_SetTime(&stTime)) {
            DBG_INFO("[PROC] Set Time Err\r\n");
        }
        DBG_INFO("[PROC] Set Time\r\n");
    }
}

static void set_net_parem(cJSON *topic){
    cJSON *timestamp = cJSON_GetObjectItem(topic, "timestamp");
    if(timestamp != NULL && timestamp->type == cJSON_Number){
        set_rtc(timestamp->valueint);
    }
    cJSON * Report = cJSON_GetObjectItem(topic, "Report");
    if (Report && Report->type == cJSON_Object){
        cJSON * enable = cJSON_GetObjectItem(Report, "enable");
        if (enable){
            WorkParam.mqtt.MQTT_DataEnable = (BOOL)enable->type;
        }
        cJSON * interval = cJSON_GetObjectItem(Report, "interval");
        if (interval){
            WorkParam.mqtt.MQTT_DataInvt = interval->valueint;
        }
    }
    cJSON * ip = cJSON_GetObjectItem(topic, "ip");
    if (ip && ip->type == cJSON_String){
        strcpy(WorkParam.mqtt.MQTT_Server, ip->valuestring);
    }
    cJSON * port = cJSON_GetObjectItem(topic, "port");
    if (port && port->type == cJSON_Number){
        WorkParam.mqtt.MQTT_Port = ip->valueint;
        //strcpy(WorkParam.mqtt.part, port->valuestring);
    }
    cJSON * heartbeat = cJSON_GetObjectItem(topic, "heartbeat");
    if (heartbeat && heartbeat->type == cJSON_Number){
        WorkParam.mqtt.MQTT_PingInvt = heartbeat->valueint;
        //strcpy(WorkParam.mqtt.part, port->valuestring);
    }
    cJSON * wifi = cJSON_GetObjectItem(topic, "WIFI");
    if (wifi && wifi->type == cJSON_Object){
        cJSON * wifireset = cJSON_GetObjectItem(wifi, "wifireset");
        if(wifireset && wifireset->type == cJSON_Number && wifireset->valueint == 1){
            CMD_Virtual("WIFI test wifirestore");
        }
        //strcpy(WorkParam.mqtt.part, port->valuestring);
    }
    WorkParam_Save();
    
}    
/**

 */
static void requestinfo(cJSON *topic)
{
    cJSON *data;
    
    if(topic && topic->type == cJSON_Object){
        data = cJSON_GetObjectItem(topic, "netreset");
        if(data && data->valueint == TRUE){
            CMD_Virtual("system reset 5000");
        }
        data = cJSON_GetObjectItem(topic, "netfactoryreset");
        if(data && data->valueint == TRUE){
            CMD_Virtual("datasave netfactoryreset 5000");
        }
        data = cJSON_GetObjectItem(topic, "netparamget");
        if(data && data->valueint == TRUE){
            net_param_updata();
        }            
        data = cJSON_GetObjectItem(topic, "allget");
        if(data && data->valueint == TRUE){
            AllInfo_Updata(TimeSwitchAdd);
        }
        data = cJSON_GetObjectItem(topic, "statusget");
        if(data && data->valueint == TRUE){
            AllInfo_Updata(NULL);
        }
        data = cJSON_GetObjectItem(topic, "countget");
        if(data && data->valueint == TRUE){
            switch_count_updata();
        }
        data = cJSON_GetObjectItem(topic, "switchinfo");
        if(data && data->valueint == TRUE){
            switchinfo_updata();
        }
        data = cJSON_GetObjectItem(topic, "timerget"); //
        if(data && data->valueint == TRUE){
            TimerInfo_Updata();
        }
    }
       
}


/**
 * 
 */
static void ArriveDFU_Data(cJSON *root)
{
    cJSON *topic = NULL;

    topic = cJSON_GetObjectItem(root, "http");
    if(topic != NULL && topic->type == cJSON_String){
        char *temp, *temp1 = NULL;
        temp = MMEMORY_ALLOC(strlen(topic->valuestring) + 11);
        if(temp != NULL){
            strcpy(temp, "DFU HTTP ");
            temp1 = strstr(topic->valuestring,"://");
            if(temp1 != NULL){
                temp1 = strstr(temp1,"/");
                
            }
            else{
                temp1 = strstr(topic->valuestring,"/");
            }
            if(temp1 != NULL){
                strncat(temp,topic->valuestring,temp1 - topic->valuestring + 1);
                strcat(temp," ");
                strcat(temp, temp1);
                CMD_Virtual(temp);
            }
            MMEMORY_FREE(temp);
        }
    }
}
/**
 *
 */
static BOOL CMD_Updata(char *cmd, cJSON *desired)
{
    BOOL ret = FALSE;
    char *s = NULL;
    cJSON *root = NULL;
    root = cJSON_CreateObject();
    if (root != NULL) {
        char temp[12];
        memset(temp, 0x00, 12);
        //uitoa(HAL_GetTick(), temp);
        cJSON_AddNumberToObject(root, "messageid", HAL_GetTick());
        cJSON_AddNumberToObject(root, "timestamp", RTC_ReadTick() - 28800);
        cJSON_AddStringToObject(root, "cmd", cmd);
        cJSON_AddStringToObject(root, "deviceid", WorkParam.mqtt.MQTT_ClientID);
        cJSON_AddItemToObjectCS(root, "desired", desired);
        s = cJSON_PrintUnformatted(root);
        if (s != NULL) {
            DBG_INFO("CMD_Updata ts:%u,data:%s", HAL_GetTick(), s);
            ret = Publish_MQTT("event/raw", QOS0, (uint8_t *)s, strlen(s));
            MMEMORY_FREE(s);
        }
        cJSON_Delete(root);
    }
    else{
        cJSON_Delete(desired);
        //cJSON_Delete(root);
    }
    return ret;
}

/**
 * 
 */
static BOOL PublishPath(char *topic, cJSON *Body)
{
    BOOL r = FALSE;
    cJSON *MBody;
    char *s;

    //DBG_LOG("[PROC] PublishPath");
    MBody = cJSON_CreateObject();
    if (MBody != NULL) {
        cJSON_AddStringToObject(MBody, "id",       WorkParam.mqtt.MQTT_ClientID);
        cJSON_AddItemToObjectCS(MBody, topic, Body);
        s = cJSON_PrintUnformatted(MBody);        //
        if (s != NULL) {
            r = Publish_MQTT("event/raw", QOS0, (uint8_t *)s, strlen(s));
            MMEMORY_FREE(s);
        }
        cJSON_Delete(MBody);
        //DBG_LOG("[PROC] PublishPath end");
    }

    return r;
}




/**
 * 
 * @param dat  
 * @param len  
 */
void ask_updata(uint32_t messageid){
    cJSON *askdesired;
    askdesired = cJSON_CreateObject();
    if (askdesired != NULL) {
        cJSON_AddNumberToObject(askdesired, "messageid", messageid);
        cJSON_AddStringToObject(askdesired, "ret", "OK");
        CMD_Updata("CMD-99", askdesired);
    }
}
/**
 * 
 * @param dat  
 * @param len  
 */
void remote_data_arriva(uint8_t *dat, uint16_t len)
{
    cJSON * root,*cmd, *desired;
    *(dat + len) = 0;
    #if AES_EN > 0   
    aes_cbc_inv_cipher_buff(dat, len, aes_get_expansion_key(), aes_get_vi());   
    #endif
    root = cJSON_Parse((const char *)dat);
    if (root != NULL) {
        static cJSON *timestamp ;
        timestamp = cJSON_GetObjectItem(root, "timestamp");
        if(timestamp != NULL && timestamp->type == cJSON_Number){
            timeRTC_t stTime;
            uint32_t rtc32 = 0; 
            rtc32 = RTC_ReadTick();
            if(abs(rtc32 - timestamp->valueint - 28800) > 10){
                RTC_TickToTime(timestamp->valueint + 28800, &stTime);
                if (FALSE == RTC_SetTime(&stTime)) {
                    DBG_INFO("[PROC] Set Time Err\r\n");
                }
                DBG_INFO("[PROC] Set Time\r\n");
            }
        }
        cJSON *messageid = cJSON_GetObjectItem(root, "messageid");
        cmd = cJSON_GetObjectItem(root, "cmd");
        cJSON * desired = cJSON_GetObjectItem(root, "desired");
        if(cmd != NULL && messageid != NULL && messageid->type == cJSON_Number && desired != NULL){
            ask_updata(messageid->valueint);
            if(strstr(cmd->valuestring, "CMD-01")){
                DBG_LOG("Request info");
                requestinfo(desired);
            }
            else if(strstr(cmd->valuestring, "CMD-03")){                
                set_switch_parem(desired);
            }
            else if(strstr(cmd->valuestring, "CMD-02")){
                set_net_parem(desired);
            }
        }        
        cJSON_Delete(root);
    }
}

/**

 */
static void process_Console(int argc, char *argv[])
{
    argv++;
    argc--;
    if (strcmp(argv[0], "publish") == 0) {
        argv++;
        argc--;
        
    }
    if (ARGV_EQUAL("info_updata")) {
        switchinfo_updata();
        net_param_updata();
        DBG_LOG("Console AllInfo_Updata\r");
    }
}
