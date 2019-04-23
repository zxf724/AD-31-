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
#include "cJSON.H"
#include "datasave.H"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
//static DeviceParam_t DeviceParam;
//static BreackerParam_t Param;

lstring g_szClientId = "ID002";
lstring g_szDomain = "www.sztosee.cn"; //
uint16 g_usPort = 1883;
static BOOL CMD_Updata(char *cmd, cJSON *desired);
 void remote_data_arriva(uint8_t *dat, uint16_t len);
/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
 * 业务处理初始
 */
void Process_Init(void) {
    //添加MQTT连接的参数即
    Subscribe_MQTT("event/raw", QOS1, FALSE, remote_data_arriva);
    DBG_LOG("Process Start.");
}
static void net_param_updata(void) {
    cJSON *desired = NULL;
    desired = cJSON_CreateObject();
    if (desired != NULL) {        
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
static BOOL CMD_Updata(char *cmd, cJSON *desired) {
    BOOL ret = FALSE;
    char *s = NULL;
    cJSON *root = NULL;
    root = cJSON_CreateObject();
    if (root != NULL) {
        char temp[12];
        memset(temp, 0x00, 12);
        //uitoa(HAL_GetTick(), temp);
        cJSON_AddNumberToObject(root, "messageid", HAL_GetTick());
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
 * @param dat  
 * @param len  
 */
void ask_updata(uint32_t messageid) {
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
void remote_data_arriva(uint8_t *dat, uint16_t len) {
    cJSON * root,*cmd, *desired;
    *(dat + len) = 0;
    #if AES_EN > 0   
    aes_cbc_inv_cipher_buff(dat, len, aes_get_expansion_key(), aes_get_vi());   
    #endif
    root = cJSON_Parse((const char *)dat);
    if (root != NULL) {
        cJSON *timestamp ;
        timestamp = cJSON_GetObjectItem(root, "timestamp");        
        cJSON *messageid = cJSON_GetObjectItem(root, "messageid");
        cmd = cJSON_GetObjectItem(root, "cmd");
        cJSON * desired = cJSON_GetObjectItem(root, "desired");
        if(cmd != NULL && messageid != NULL && messageid->type == cJSON_Number && desired != NULL){
            ask_updata(messageid->valueint);
            if(strstr(cmd->valuestring, "CMD-01")){
                DBG_LOG("Request info");                
            }
            else if(strstr(cmd->valuestring, "CMD-03")){                
                
            }
            else if(strstr(cmd->valuestring, "CMD-02")){
               
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
        net_param_updata();
        DBG_LOG("Console AllInfo_Updata\r");
    }
}
