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
#include "console.h"
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
    cJSON * root,*cmd ;
    *(dat + len) = 0;
    static uint8_t byte2,byte3;
    #if AES_EN > 0
    aes_cbc_inv_cipher_buff(dat, len, aes_get_expansion_key(), aes_get_vi());
    #endif
    root = cJSON_Parse((const char *)dat);
    if (root != NULL) {
        cJSON *devicework ;
        devicework = cJSON_GetObjectItem(root, "devicework");
        cJSON *Time ;
        Time = cJSON_GetObjectItem(root, "time");
        cmd = cJSON_GetObjectItem(root, "cmd");
        cJSON *messageid;
        messageid = cJSON_GetObjectItem(root, "messageid");

        if(cmd != NULL && devicework != NULL && devicework->type == cJSON_Number && Time != NULL) {
            ask_updata(messageid->valueint);
            if(strstr(cmd->valuestring, "CMD-101")) {       //101 massage time setting 
                DBG_LOG("boot,shut down or suspend");
                if(strstr(devicework->valuestring, "1")) {
                    byte3 = (Time->valueint + 10);
                    DBG_LOG("massage time is %d",Time->valueint);
                }
                if(strstr(devicework->valuestring, "0")) {
                    DBG_LOG("device shut down!");
                }
                if(strstr(devicework->valuestring, "2")) {
                    DBG_LOG("device suspend!");
                }
                byte2 = 0;
                CommunicationToControlPoll(byte2,byte3);
                DBG_LOG("massage time is %d mins",dat[3]);
            }
            else if(strstr(cmd->valuestring, "CMD-102")){   // no use those yet
                    DBG_LOG("mode change, now is 0 is feasible!");
                    cJSON *mode ;
                    mode = cJSON_GetObjectItem(root,"mode");
                    if(mode != NULL) {
                        byte2 = mode->valueint;
                    }
                    CommunicationToControlPoll(byte2,byte3);
            }
            else if(strstr(cmd->valuestring, "CMD-103")){
                    DBG_LOG("change the chair");
                    cJSON *control;
                    control = cJSON_GetObjectItem(root, "control");
                    byte2 = 0;
                    byte3 = control->valueint;
                    CommunicationToControlPoll(byte2,byte3);
            }
        }
        CommunicationToControlPoll(byte2,byte3);
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


/**
 *  control board to communication board
 */
void ControlToCommunicationPoll(uint8_t *dat) {
    //in this case, byte5,byte6 means left message time
    static uint16_t gs_byte5_byte6 = 0;
    cJSON *sit;
    uint8_t tmp_dat = 0;

    for(uint8_t i=0;i<=7;i++) {
        tmp_dat += dat[i];
    }
    if(tmp_dat == dat[8]) {
        DBG_LOG("dat check is corrcet!");
        gs_byte5_byte6 = (dat[5] << 8) | dat[6];
        DBG_LOG("gs_byte5_byte6 is %04x",gs_byte5_byte6);
        if(gs_byte5_byte6 > 0) {
            cJSON_AddNumberToObject(sit, "sit", 1);
            CMD_Updata("CMD-03",sit);
        }
    }
    delay(200);
}

/**
 * communication board to control board
 */
void CommunicationToControlPoll(uint8_t byte2 , uint8_t byte3) {
    static uint8_t dat[5] = {0x06,0x01,0x00,0x28,0x2F};     // 11 = 1min +10
    dat[2] = byte2;
    dat[3] = byte3;
    // DBG_LOG("dat[3] = %02x",dat[3]);
    //check digit
    dat[4] = dat[0] + dat[1] + dat[2] + dat[3];
    CMD_PipeSendData(2,dat,sizeof(dat));  //send data to board
}
