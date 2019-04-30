
/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file protocols.c
 * @version V1.0
 * @date 2016.4.1
 * @brief 电箱与服务器端通讯协议函数文件.
 *
 * *********************************************************************
 * @note
 *
 * *********************************************************************
 * @author 宋阳
 * 2016.12.30 增加订阅失败重连
 */



/* Includes ------------------------------------------------------------------*/
#include "user_comm.h"
#include "MQTTClient.h"
#include "mqtt_conn.h"
#include "cjson.h"
#include "datasave.h"
#include "process.h"

#if AES_EN > 0
#include "aes_ex.h"
#endif
/* Private typedef -----------------------------------------------------------*/
typedef struct
{
    Qos qos;
    char const *topic;
    BOOL isBroadcast;
    char topicBuf[MQTT_TOPIC_MAX_LEN];
    Arrived_t callback;
} SubScribe_t;

/* Private define ------------------------------------------------------------*/
#define mqttPar     (WorkParam.mqtt)

/* Private macros ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static osMessageQId MQTT_SendQId;

static uint8_t txBuffer[MQTT_TX_BUFF_SIZE], rxBuffer[MQTT_RX_BUFF_SIZE];

static uint16_t Publish_Fail = 0, Connect_Fail = 0;
static uint32_t tsPingOut = 0;

static MQTTClient mClient;
static Network mNetwork;
static MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

static SubScribe_t Subs[MAX_MESSAGE_HANDLERS];
static uint8_t DataFlowCnt = 0;

static MQTT_TaskPollFun pollFun;
static MQTT_MsgDataFun sendfailFun;

/* Private function prototypes -----------------------------------------------*/
void MQTT_Conn_Task(void const *argument);
void messageArrived(MessageData *data);
static void Manager_MQTT(void);
static void MQTT_SendPoll(void);
static void MQTT_ReSubscribe(void);
static BOOL Connect_MQTT(void);
static BOOL Disconnect_MQTT(void);
static void mqtt_Console(int argc, char *argv[]);


/* Exported functions --------------------------------------------------------*/

/**
 * 协议处理初始
 */
void MQTT_Conn_Init(void)
{

    osMessageQDef(MQTT_SendQ, MQTT_SEND_Q_SIZE, void *);
    MQTT_SendQId = osMessageCreate(osMessageQ(MQTT_SendQ), NULL);

    osThreadDef(mqtt_conn, MQTT_Conn_Task, MQTT_TASK_PRIO, 0, MQTT_TASK_STK_SIZE);
    osThreadCreate(osThread(mqtt_conn), NULL);

    CMD_ENT_DEF(mqtt, mqtt_Console);
    Cmd_AddEntrance(CMD_ENT(mqtt));

    DBG_LOG("Protocl init OK.");
}

/**
 * MQTT设置勾子函数
 * @param poll    任务轮询勾子函数
 * @param msgdata 数据发送失败勾子函
 */
void MQTT_SetHookFun(MQTT_TaskPollFun poll, MQTT_MsgDataFun sendfail)
{
    MCPU_ENTER_CRITICAL();
    pollFun = poll;
    sendfailFun = sendfail;
    MCPU_EXIT_CRITICAL();
}

/**
 * 协议处理的任
 * @param argument
 */
void MQTT_Conn_Task(void const *argument)
{
    TWDT_DEF(mqttConTask, 60000);
    TWDT_ADD(mqttConTask);
    TWDT_CLEAR(mqttConTask);

    NetworkInit(&mNetwork);
    MQTTClientInit(&mClient, &mNetwork, MQTT_TIMEOUT_DEF, txBuffer, sizeof(txBuffer), rxBuffer, sizeof(rxBuffer));


#if MQTT_ALIYUN_EN > 0
    M2M_SetOnOff(TRUE);

    int i = 0;

    while (i < 30) {
        osDelay(1000);
        i++;
        if (M2M_ReadStatus() == M2M_status_online) {
            break;
        }
    }
    DBG_LOG("MQTT init, aliyun auth.");
    Aliyun_Auth();
#endif

    DBG_LOG("Protocl task start.");
    while (1) {
        TWDT_CLEAR(mqttConTask);

        Manager_MQTT();
        if (pollFun != NULL) {
            DBG_LOG("pollFun.\r");
            pollFun();
            
        }
        if (System_SockIsConnected(NULL, NULL) > 0) {
            MQTTYield(&mClient, 10);        //log_in set
            MQTT_SendPoll();

        }
        //DBG_LOG("[PMET] MQTT_Conn_Task\r");
        osDelay(2);
    }
}

/**
 * MQTT连接状态查,无连接返，已连接返回MQTT使用的网络路
 */
BOOL MQTT_IsConnected(void)
{
    return (BOOL)mClient.isconnected;
}

/**
 * MQTT数据传输状态查
 */
BOOL MQTT_IsDataFlow(void)
{
    return (BOOL)DataFlowCnt;
}

/**
 * 处理订阅的消
 * @param data 消息数据
 */
void messageArrived(MessageData *data)
{
    int i = 0;

#if MQTT_DEBUG > 0

    DBG_DBG("Receive New message:%d, payload:%s\n",
            data->topicName->lenstring.len,
            data->topicName->lenstring.data);
#if MQTT_DEBUG > 2
    CMD_HEX_Print(data->message->payload, data->message->payloadlen);
#endif
#endif
    #if MQTT_TYPE == 0
    for (i = 0; i < MAX_MESSAGE_HANDLERS; i++) {
        if (Subs[i].topic != NULL) {
            if (strncmp(Subs[i].topicBuf, data->topicName->lenstring.data, data->topicName->lenstring.len) == 0
                && Subs[i].callback != NULL) {
                Subs[i].callback(data->message->payload, data->message->payloadlen);
            }
        }
    }
    #elif MQTT_TYPE == 1
    for (i = 0; i < MAX_MESSAGE_HANDLERS; i++) {
        if (Subs[i].callback != NULL) {
            Subs[i].callback(data->message->payload, data->message->payloadlen);
        }
    }
    #endif
}

/**
 * 协议处理发送数
 * @param dat  数据指针
 * @param len  数据长度
 */
int16_t MQTT_SendData(uint8_t *dat, uint16_t len)
{
    int16_t rc = -1;

    if (System_SockIsConnected(NULL, NULL) > 0 && System_SockIsLock() == FALSE) {
        /*用于网络指示*/
        DataFlowCnt += 1;

        rc = System_SockSend(dat, len);
#if MQTT_DEBUG > 1
        if (rc != 0) {
            DBG_DBG("MQTT send data, rc:%d", rc);
            CMD_SendData((uint8_t *)dat, len);
        }
#if MQTT_DEBUG > 2
        CMD_HEX_Print(dat, len);
#endif
#endif
    }
    return rc;
}

/**
 * 协议处理读出数据
 * @param dat  数据读出的指
 * @param len  待读出的长度
 * @return 返回实际读出的长
 */
int16_t MQTT_ReadData(uint8_t *dat, uint16_t len)
{
    int16_t rc = 0;

    if (System_SockIsLock() == FALSE) {
        rc = System_SockRecv(dat, len);

        if (rc > 0) {
#if MQTT_DEBUG > 1
            DBG_LOG("MQTT read data, len:%d", rc);
#if MQTT_DEBUG > 2
            CMD_HEX_Print(dat, len);
#endif
#endif
            DataFlowCnt += 1;
        }
    }
    return rc;
}

/**
 * 发布MQTT消息
 * @param topic   发布的主题名
 * @param qos     消息服务质量等级
 * @param payload 消息
 * @param len     消息体长
 * @return 返回发布结果
 */
BOOL Publish_MQTT(char const *topic, Qos qos, uint8_t *payload, uint16_t len)
{
    #if MQTT_TYPE == 0
    MQTTMessage *pmsg;
    
    osStatus res = osOK;
    uint8_t *pBuf = NULL, toplen;
    char *top = NULL;
    #if MQTT_ALIYUN_EN  > 0
    toplen = strlen(WorkParam.aliyun.productKey) + strlen(topic) + 5;
    #else
    toplen = strlen(topic) + 5 + 32;
    #endif
    
    uint8_t *ptemppayload = NULL;
    ptemppayload = payload;
    
    #if AES_EN > 0
    DBG_DBG("payload, len:%d", len);
    uint8_t temp = 0;
    temp = len % 16;
    DBG_DBG("payload, len:%d,temp:%d\r", len,temp);
    if(temp != 0){   
        temp = 16 - temp;
        ptemppayload = MMEMORY_ALLOC(len + temp);
        if(ptemppayload != NULL){
            memcpy(ptemppayload, payload, len);
            for(uint8_t i = 0; i < temp; i++){
                ptemppayload[len + i] = temp;
            }
            len += temp;
        }
    }
    aes_cbc_cipher_buff(ptemppayload, len, aes_get_expansion_key(), aes_get_vi());
    #endif
    pBuf = MMEMORY_ALLOC(len + sizeof(MQTTMessage) + toplen);
    if (pBuf != NULL) {
        memset(pBuf, 0, len + sizeof(MQTTMessage) + toplen);
        //DBG_LOG("[MQTT] >>>0");
        pmsg = (MQTTMessage *)pBuf;
        pmsg->qos = qos;
        pmsg->retained = 0;
        pmsg->payload = pBuf + sizeof(MQTTMessage);
        pmsg->payloadlen = len;

        memcpy(pmsg->payload, ptemppayload, len);
        #if AES_EN > 0
        if(ptemppayload){
            MMEMORY_FREE(ptemppayload);
        }
        #endif
        top = (char *)pmsg->payload + pmsg->payloadlen;
        top[0] = 0;
        #if MQTT_ALIYUN_EN  > 0
        top[0] = '/';
        strcpy(top + 1, &WorkParam.aliyun.productKey[0]);
        strcat(top, "/");
        #else
        strcat(top, WorkParam.mqtt.MQTT_ClientID);
        strcat(top, "/");
        #endif
        strcat(top, topic);

        res = osMessagePut(MQTT_SendQId, (uint32_t)pBuf, 1000);
        if (res == osOK) {
            DBG_DBG("MQTT message Q   OK.");
            return TRUE;
        } else {
            DBG_WAR("MQTT message Q fault:%d", (int)res);
            MMEMORY_FREE(pBuf);
        }
    }
    else{
        #if AES_EN > 0
      if(ptemppayload){
            MMEMORY_FREE(ptemppayload);
        }
        #endif
    }
#elif MQTT_TYPE == 1
    MQTTMessage *pmsg;
    osStatus res = osOK;
    uint8_t *pBuf = NULL;
    pBuf = MMEMORY_ALLOC(len + sizeof(MQTTMessage) + 1); //
    if (pBuf != NULL) {
        memset(pBuf, 0, len + sizeof(MQTTMessage) + 1);
        pmsg = (MQTTMessage *)pBuf;
        pmsg->qos = qos;
        pmsg->retained = 0;
        pmsg->payload = pBuf + sizeof(MQTTMessage);
        pmsg->payloadlen = len;
        memcpy(pmsg->payload, payload, len);
        //DBG_LOG("[MQTT] MQTT message %s",pBuf);
        res = osMessagePut(MQTT_SendQId, (uint32_t)pBuf, 1000);
        //DBG_LOG("[MQTT] DataSend len:%d:%s",pmsg->payloadlen,pmsg->payload);
        if (res == osOK) {
            DBG_DBG("MQTT message Q OK.");
            return TRUE;
        } else {
            DBG_WAR("MQTT message Q fault:%d", (int)res);
            MMEMORY_FREE(pBuf);
        }
        //DBG_LOG("[MQTT] MQTT message end");
    }
#endif
    return FALSE;
}

/**
 * 订阅MQTT消息，须在MQTT连接之前调用此函
 * @param topic 订阅的主题名
 * @param qos   消息服务质量等级
 * @param fun   订阅消息回调的指
 * @return 返回订阅结果
 */
BOOL Subscribe_MQTT(char const *topic, Qos qos, BOOL isBroadcast, Arrived_t fun)
{
    uint8_t i = 0;

    if (topic != NULL && fun != NULL) {
        for (i = 0; i < MAX_MESSAGE_HANDLERS; i++) {
            if (Subs[i].topic == NULL) {
                Subs[i].topic = topic;
                Subs[i].qos = qos;
                Subs[i].isBroadcast = isBroadcast;
                Subs[i].callback = fun;
                return TRUE;
            }
        }
    }
    return FALSE;
}

/* Private function prototypes -----------------------------------------------*/

/**
 * MQTT连接管理
 */
static void Manager_MQTT(void)
{
    BOOL pingout = FALSE;
    static int8_t pathS = 0, flowcnt = 0;
    static uint32_t tsDataFlow = 0, tsConnect_Fail = 0;

    char *addr = NULL;
    int8_t path = 0;
    path = 0;
    uint16_t port = 0;

    /*网络传输计数，用于网络指示灯*/
    if (DataFlowCnt > 0 && flowcnt == 0) {
        flowcnt = 1;
        TS_INIT(tsDataFlow);
    }
    if (TS_IS_OVER(tsDataFlow, 100) && DataFlowCnt > 0) {
        DataFlowCnt = 0;
        flowcnt = 0;
    }

    //  add addr port , it is true
    addr = WorkParam.mqtt.MQTT_Server;
    port = WorkParam.mqtt.MQTT_Port;

    /*网络类型变化或者掉线时应断开MQTT，否则在线状态是假的*/
    path = System_SockIsConnected(&addr, &port);
    DBG_LOG("path is %d",path);

    if (pathS != path || addr != mqttPar.MQTT_Server || port != mqttPar.MQTT_Port) {
        pathS = path;
        if (mClient.isconnected) {
            MCPU_ENTER_CRITICAL();
            mClient.isconnected = 0;
            MCPU_EXIT_CRITICAL();
        }
    }

    /*推送失败或心跳无应答时断开连接*/
    if (mClient.ping_outstanding == 0) {
        tsPingOut = HAL_GetTick();
    } else if (HAL_GetTick() - tsPingOut >= MQTT_TIMEOUT_DEF) {
        pingout = TRUE;
    }
    if (Publish_Fail > 0 || pingout > 0) {
        portENTER_CRITICAL();
        Publish_Fail = 0;
        mClient.ping_outstanding = 0;
        portEXIT_CRITICAL();
        if (mClient.isconnected) {
            Disconnect_MQTT();
        }
    }
    /*长时间连不上socket需重新签权*/
    if (TS_IS_OVER(tsConnect_Fail, CONNECT_FAIL_TIMEOUT * 1000)) {
        TS_INIT(tsConnect_Fail);
        if (path == 0) {
            Connect_Fail++;
        }
    }

    /*多次连接失败后重新鉴*/
    if (Connect_Fail >= CONNECT_FAIL_REAUTH) {
        Connect_Fail = 0;
#if MQTT_ALIYUN_EN > 0
        DBG_LOG("MQTT connect failed, do aliyun auth.");
        Aliyun_Auth();
#endif
    }

    /*已有服务器连接参*/
    if (PARAM_LEGAL(mqttPar.MQTT_Server) && mqttPar.MQTT_Port != 0){
        /*无连接时开始连*/
        if (mClient.isconnected == 0) {
            if (System_SockIsLock() == FALSE) {
                System_SetSocket(mqttPar.MQTT_Server, mqttPar.MQTT_Port);
            }
            /*建立连接*/
            if (path > 0 && addr == mqttPar.MQTT_Server && port == mqttPar.MQTT_Port) {
                /*参数初始*/
                //DBG_LOG("MQTT_Server,MQTT_Port:%s,%d", mqttPar.MQTT_Server,mqttPar.MQTT_Port);
                connectData.MQTTVersion = 4;
                connectData.keepAliveInterval = MQTT_PING_INVT_DEF;
                connectData.clientID.cstring = WorkParam.mqtt.MQTT_ClientID;
                connectData.username.cstring = WorkParam.mqtt.MQTT_UserName;
                connectData.password.cstring =  WorkParam.mqtt.MQTT_PassWord;
                connectData.cleansession = 1;
                connectData.willFlag = 0;

                /*连接MQTT*/
                if (Connect_MQTT()) {
                    DBG_LOG("MQTT-clientId:%s", mqttPar.MQTT_ClientID);
                    //DBG_LOG("MQTT-userName:%s", mqttPar.MQTT_UserName);
                    DBG_LOG("MQTT Ping invter %d s", MQTT_PING_INVT_DEF / 2);
                    DBG_LOG("MQTT_Server,MQTT_Port:%s,%d", mqttPar.MQTT_Server,mqttPar.MQTT_Port);
                    Connect_Fail = 0;
                    MQTT_ReSubscribe();
                    
                    //test
                    cJSON *desired = NULL;
                    desired = cJSON_CreateObject();
                    if(desired != NULL) {
                        cJSON_AddStringToObject(desired, "test-liuxiong", "connect-ok");
                    }
                    CMD_Updata("CMD-001",desired);

                } else {
                    DBG_LOG("Connect_Fail");
                    Connect_Fail++;
                }
            }
        }
    }
    /*服务器为空时执行鉴权*/
    else {
#if MQTT_ALIYUN_EN > 0
        DBG_LOG("MQTT Param empty, do aliyun auth.");
        Aliyun_Auth();
#endif
    }
}

/**
 * MQTT推送轮��?
 * @param id   MQTT的ID
 * @return 返回连接结果
 */
static void MQTT_SendPoll(void)
{
    int rc = 0, fail = 0;
    char *top = NULL;
    osEvent evt;
    MQTTMessage *pmsg;

    rc = rc;
    evt = osMessageGet(MQTT_SendQId, 2);
    if (evt.status == osEventMessage) {
        pmsg = evt.value.p;
        if (pmsg->payloadlen > 0) {
            #if MQTT_TYPE == 0
            top = (char *)pmsg->payload + pmsg->payloadlen;
            #endif
            if (MQTT_IsConnected()) {
                if ((rc = MQTTPublish(&mClient, top, pmsg)) != SUCESS) {
                    Publish_Fail++;
                    fail++;
                } else {
                    Publish_Fail = 0;
                }
                DBG_LOG("MQTT publish top:%s, return:%d,pmsg->payloadlen:%d", top, rc,pmsg->payloadlen);
#if MQTT_DEBUG > 0
                //
#if MQTT_DEBUG > 2
                CMD_HEX_Print(pmsg->payload, pmsg->payloadlen);
#endif
#endif
            } else {
                fail++;
#if MQTT_DEBUG > 0
                DBG_LOG("MQTT publish fail, MQTT not connect.");
#endif
            }
            if (fail > 0) {
                if (sendfailFun != NULL) {
                    sendfailFun(top, (uint8_t *)(pmsg->payload), pmsg->payloadlen);
                }
            }
            //MMEMORY_FREE(evt.value.p);
        }
        MMEMORY_FREE(evt.value.p); // Å·Ñô
    }
}

/**
 * MQTTé‡æ–°è®¢é˜…ä¸»é¢˜ ç±»MQTTï¼šæ³¨å†Œå¤„ç†å›žï¿½ï¿½?
 */
static void MQTT_ReSubscribe(void)
{
    int i, rc;

    portENTER_CRITICAL();
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i) {
        mClient.messageHandlers[i].topicFilter = 0;
    }
    portEXIT_CRITICAL();

    /*已订阅的主题重新订阅*/
    for (i = 0; i < MAX_MESSAGE_HANDLERS; i++) {
        if (Subs[i].topic != NULL) {
            Subs[i].topicBuf[0] = 0;
#if MQTT_ALIYUN_EN  > 0
            /*阿里云topic前面为productKey*/
            Subs[i].topicBuf[0] = '/';
            strcpy(&Subs[i].topicBuf[1], WorkParam.aliyun.productKey);
            strcat(Subs[i].topicBuf, "/");
#endif
            if (Subs[i].isBroadcast == FALSE) {
                strcat(Subs[i].topicBuf, WorkParam.mqtt.MQTT_ClientID);
                strcat(Subs[i].topicBuf, "/");
            }
            strcat(Subs[i].topicBuf, "notify/raw");
            rc = MQTTSubscribe(&mClient, Subs[i].topicBuf, Subs[i].qos, messageArrived);
#if MQTT_DEBUG > 0
            DBG_LOG("MQTT subscribe %s, return:%d", Subs[i].topicBuf, rc);
#endif
            if (rc != 0) {
                Publish_Fail++;
                break;
            }
        }
    }
}

/**
 * 连接至MQTT服务
 * @param id   MQTT的ID
 * @return 返回连接结果
 */
static BOOL Connect_MQTT(void)
{
    int rc = 0;
    #if MQTT_TYPE == 1
    MQTT_ReSubscribe();    
    #endif
    /*连接失败时重试一��?*/
    rc = MQTTConnect(&mClient, &connectData);
    if (rc != SUCESS) {
        rc = MQTTConnect(&mClient, &connectData);
    }
    if (rc != SUCESS) {
        DBG_LOG("Return code from MQTT connect is %d", rc);
    } else {
        DBG_LOG("MQTT Connected");
    }
    return (BOOL)!rc;
}

/**
 * 断开MQTT的连��?
 * @return 返回断开结果
 */
static BOOL Disconnect_MQTT(void)
{
    int rc;

    if ((rc = MQTTDisconnect(&mClient)) != SUCESS) {
        DBG_LOG("Return code from MQTT disconnect is %d", rc);
    } else {
        DBG_LOG("MQTT Disonnected");
    }

    return (BOOL)!rc;
}

/**
 * 调试命令
 * @param argc 参数项数��?
 * @param argv 参数列表
 */
static void mqtt_Console(int argc, char *argv[])
{
    Qos qos;

    argv++;
    argc--;
    if (strcmp(argv[0], "MQTT") == 0) {
        argv++;
        argc--;
        if (strcmp(argv[0], "publish") == 0) {
            qos = (Qos)uatoi(argv[2]);
            if (Publish_MQTT(argv[1], qos, (uint8_t *)argv[3], strlen(argv[3]))) {
                DBG_LOG("MQTT publish %s OK.", argv[1]);
            }
        } else if (strcmp(argv[0], "status") == 0) {
            DBG_LOG("MQTT client connect:%d, ping out:%d, Net path:%d.",
                    mClient.isconnected,
                    mClient.ping_outstanding,
                    System_SockIsConnected(NULL, NULL));
        } else if (strcmp(argv[0], "ping") == 0) {
            TimerInit(&mClient.ping_timer);
            TimerCountdownMS(&mClient.ping_timer, 100);
            DBG_LOG("test ping OK.");
        }
    }
#if MQTT_ALIYUN_EN  > 0
    if (strcmp(argv[0], "aliyun") == 0) {
        if (strcmp(argv[1], "auth") == 0) {
            Aliyun_Auth();
            DBG_LOG("Aliyun auth OK.");
        }
    }
#endif
}
