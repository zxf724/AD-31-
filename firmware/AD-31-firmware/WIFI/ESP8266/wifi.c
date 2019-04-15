/**
 * *********************************************************************
 *             Copyright (c) 2017 AFU All Rights Reserved.
 * @file wifi.c
 * @version V1.0
 * @date 2017.7.14
 * @brief WIFI模块驱动及管理函数文件,用于ESP8266.
 *
 * *********************************************************************
 * @note
 *
 * *********************************************************************
 * @author 宋阳
 */



/* Includes ------------------------------------------------------------------*/
#include "user_comm.h"
#include "main.h"
#include "wifi.h"
/* Private typedef -----------------------------------------------------------*/
typedef struct
{
    char        *ssid;
    char        *pwd;
    char        *MAC;
    char        *IP;
    char        *SN;
    char        *GW;
    BOOL        DHCP_Enable;
    char       *ConnectAddress;
    uint16_t    ConnectPort;
    Sock_RecCBFun callback;
    int8_t      ConnFail;
    uint8_t     rssi;
    uint16_t    ErrorCount;
    WIFI_Status_t status;
    BOOL        powerOnOff;
} WIFI_Param_t;

/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/
#define WIFI_PWR_ON()                   HAL_GPIO_WritePin(WIFI_POWER_EN_GPIO_Port, WIFI_POWER_EN_Pin, GPIO_PIN_SET) //IO_H有被优化的可能
#define WIFI_PWR_OFF()                  HAL_GPIO_WritePin(WIFI_POWER_EN_GPIO_Port, WIFI_POWER_EN_Pin, GPIO_PIN_RESET)

#define WIFI_RST_ON()                   HAL_GPIO_WritePin(WIFI_RST_GPIO_Port, WIFI_RST_Pin, GPIO_PIN_RESET)
#define WIFI_RST_OFF()                  HAL_GPIO_WritePin(WIFI_RST_GPIO_Port, WIFI_RST_Pin, GPIO_PIN_SET)

#define WIFI_SEND_DATA(dat, len)        UART_SendData(WIFI_UART_PORT, dat, len)
#define WIFI_SEND_AT(cmd)               UART_Printf(WIFI_UART_PORT, "AT+%s\r\n", cmd)
#define WIFI_AT_PRINTF(format, ...)     UART_Printf(WIFI_UART_PORT, "AT+"format"\r\n", ##__VA_ARGS__)

#define WIFI_WAIT_ACK(token, timeout)   WIFI_WaitATRsp(token"\r\n", timeout)
#define WIFI_WAIT_TOKEN(token, timeout) WIFI_WaitATRsp(token, timeout)

#define WIFI_OPT_SET(opt)               do {MCPU_ENTER_CRITICAL(); MASK_SET(WIFI_Opt, opt); MCPU_EXIT_CRITICAL();}while(0)
#define WIFI_OPT_CLEAR(opt)             do {MCPU_ENTER_CRITICAL(); MASK_CLEAR(WIFI_Opt, opt); MCPU_EXIT_CRITICAL();}while(0)
#define WIFI_OPT(opt)                   IS_MASK_SET(WIFI_Opt, opt)

/* Private variables ---------------------------------------------------------*/
static osMessageQId WIFI_TCPIP_SendQId;
static WIFI_Param_t WIFI_Param;
static uint32_t WIFI_Opt;

#if WIFI_CMD_EN > 0
static uint8_t WIFI_Pipe = 0;
#endif

static osMutexId WIFI_MutexId;

static uint8_t *pRspBuf = NULL;

/* Private function prototypes -----------------------------------------------*/
void WIFI_Task(void const *argument);
static void WIFI_ManagerPoll(void);
static void WIFI_Socket_SendProc(void);
static void WIFI_EventProc(void);
static void WIFI_GetIPStatus(void);
static char* WIFI_Scoket_ReceiveProc(char *data);
static void WIFI_ModuleInit(void);
static BOOL WIFI_Socket_Send(uint8_t *data, uint16_t len);
static BOOL WIFI_ScoketConnect(char *addr, uint16_t port);
static void WIFI_SocketClose(void);
static BOOL WIFI_EnterSmartConfig(void);
static BOOL WIFI_ExitSmartConfig(void);
static int8_t WIFI_GetRSSI(void);
static uint8_t WIFI_SeekSSID(char *ssid);
static char* WIFI_WaitATRsp(char *token, uint32_t time);
static void WIFI_Console(int argc, char *argv[]);

/* Exported functions --------------------------------------------------------*/
/**
 * WIFI模块初始化
 */
void WIFI_Init(void)
{
    WIFI_Param.status = wifi_status_poweroff;

    osMutexDef(WIFI);
    WIFI_MutexId = osMutexCreate(osMutex(WIFI));

    osMessageQDef(WIFI_SendQ, WIFI_SEND_Q_SIZE, void *);
    WIFI_TCPIP_SendQId = osMessageCreate(osMessageQ(WIFI_SendQ), NULL);

    osThreadDef(WIFI, WIFI_Task, WIFI_TASK_PRIO, 0, WIFI_TASK_STK_SIZE);
    osThreadCreate(osThread(WIFI), NULL);

#if WIFI_CMD_EN > 0
    WIFI_Pipe = CMD_Pipe_Register((CMD_SendFun)WIFI_SocketSendData);
    DBG_LOG("WIFI CMD pipe is %d.", WIFI_Pipe);
#endif

    CMD_ENT_DEF(WIFI, WIFI_Console);
    Cmd_AddEntrance(CMD_ENT(WIFI));
    DBG_LOG("WIFI Init.");
}

/**
 * WIFI任务
 * @param argument 初始化参数
 */
void WIFI_Task(void const *argument)
{
    TWDT_DEF(wifiTask, 30000);
    TWDT_ADD(wifiTask);
    TWDT_CLEAR(wifiTask);
    DBG_LOG("WIFI Task Start.");
    while (1) {
        osDelay(5);
        TWDT_CLEAR(wifiTask);

        if (osMutexWait(WIFI_MutexId, 1000) == osOK) {
            WIFI_ManagerPoll();
            WIFI_Socket_SendProc();
            WIFI_EventProc();

            if (pRspBuf != NULL) {
                MMEMORY_FREE(pRspBuf);
                pRspBuf = NULL;
            }
            osMutexRelease(WIFI_MutexId);
        }
    }
}

/**
 * WIFI模块重启
 */
void WIFI_ReStart(void)
{
    if (!WIFI_OPT(WIFI_OPT_RESET)) {
        WIFI_OPT_SET(WIFI_OPT_RESET);
    }
}

/**
 * WIFI模块开关机
 */
void WIFI_SetOnOff(BOOL onoff)
{
    if (WIFI_Param.powerOnOff != onoff) {
        MCPU_ENTER_CRITICAL();
        WIFI_Param.powerOnOff = onoff;
        MCPU_EXIT_CRITICAL();

        if (onoff == FALSE) {
            WIFI_OPT_SET(WIFI_OPT_RESET);
        }
    }
}

/**
 * 读WIFI模块的工作状态
 * @return 返回WIFI模块的工作状态
 */
WIFI_Status_t WIFI_ReadStatus(void)
{
    return WIFI_Param.status;
}

/**
 * WIFI socket 发送数据
 * @param data 数据指针
 * @param len  数据的长度
 * @return 返回发送结果
 */
int16_t WIFI_SocketSendData(uint8_t *data, uint16_t len)
{
    osStatus res = osOK;
    uint8_t *pBuf = NULL, *p = NULL;

    if (len > WIFI_SEND_MAX_SIZE) {
        len = WIFI_SEND_MAX_SIZE;
    }

    if (WIFI_Param.status == wifi_status_socket_connected) {
        pBuf = pvPortMalloc(len + 2);
        if (pBuf != NULL) {
            p = pBuf;
            *(uint16_t *)p = len;
            p += 2;
            memcpy(p, data, len);
            //DBG_LOG("WIFI_SocketSendData:%d", len);
            res = osMessagePut(WIFI_TCPIP_SendQId, (uint32_t)pBuf, 1000);
            if (res == osOK) {
                return len;
            } else {
                DBG_DBG("WIFI socket Send Q fault:%d", (int)res);
                vPortFree(pBuf);
                return 0;
            }
        }
    }
    return -1;
}

/**
 * 查询WIFI模块socket是否已连接
 */
int8_t WIFI_IsSocketConnect(void)
{
    int8_t ret = -1;

    if (WIFI_Param.status != wifi_status_fault) {
        ret = 0;
    }

    if (WIFI_Param.status == wifi_status_socket_connected
        && !WIFI_OPT(WIFI_OPT_SET_SOCKET)) {
        ret = 1;
    }
    return ret;
}

/**
 * 设置Socket参数
 * @param server 服务器的地址
 * @param port   端口号
 */
void WIFI_SetSocketParam(char *server, uint16_t port, Sock_RecCBFun callback)
{
    if (server != WIFI_Param.ConnectAddress || port != WIFI_Param.ConnectPort || callback != WIFI_Param.callback) {
        MCPU_ENTER_CRITICAL();
        WIFI_Param.ConnectAddress = server;
        WIFI_Param.ConnectPort = port;
        WIFI_Param.callback = callback;
        WIFI_Opt |= WIFI_OPT_SET_SOCKET;
        MCPU_EXIT_CRITICAL();
    }
    if (server != NULL && port > 0) {
        /*WIFI开机*/
        WIFI_SetOnOff(TRUE);
    }
}

/**
 * 读WIFI模块的RSSI值
 * @return 返回RSSI值
 */
uint8_t WIFI_ReadRSSI(void)
{
    uint8_t rssi = 0;

    if (osMutexWait(WIFI_MutexId, 1000) == osOK) {
        rssi = WIFI_GetRSSI();
        WIFI_Param.rssi = rssi;
        osMutexRelease(WIFI_MutexId);
    } else {
        rssi = WIFI_Param.rssi;
    }
    return rssi;
}

/**
 * 设置WIFI站点的参数，为NULL只使用EASYLINK连接
 * @param ssid 路由器的SSID
 * @param pwd  路由器的密码
 */
void WIFI_SetStation(char *ssid, char *pwd)
{
    if (WIFI_Param.ssid != ssid || WIFI_Param.pwd != pwd) {
        MCPU_ENTER_CRITICAL();
        WIFI_Param.ssid = ssid;
        WIFI_Param.pwd = pwd;
        WIFI_Opt |= WIFI_OPT_SET_STATION;
        MCPU_EXIT_CRITICAL();
    }
}
static uint32_t tsElink = 0 ,  tsStation = 0;
/* Private function prototypes -----------------------------------------------*/
/**
 * WIFI 工作状态管理
 */
static void WIFI_ManagerPoll(void)
{
    static uint32_t tsNotConnect = 0;

    /*开机管理*/
    if (WIFI_Param.status == wifi_status_poweroff) {
        if (WIFI_Param.powerOnOff) {
            WIFI_ModuleInit();
            //DBG_LOG("WIFI module power on init.");
        }
    }
    /*路由器无连接时进入配网*/
    if (WIFI_Param.status == wifi_status_station_notconnected) {
        if (TS_IS_OVER(tsStation, WIFI_STA_TIMOUT * 1000)) {
            TS_INIT(tsElink);
            TS_INIT(tsStation);
            WIFI_Param.status = wifi_status_elink;
            for(uint8_t i = 0; i < 2; i++){
                if(WIFI_EnterSmartConfig())break;
            }
            
        }
    } else {
        TS_INIT(tsStation);
    }
    /*配网超时尝试重启重新连接*/
    if (WIFI_Param.status == wifi_status_elink) {
        if (TS_IS_OVER(tsElink, WIFI_ELINK_TIMEOUT * 1000)) {
            TS_INIT(tsStation);
            TS_INIT(tsElink);
            WIFI_ExitSmartConfig();
            WIFI_ReStart();
        }
    } else {
        TS_INIT(tsElink);
    }

    /*长时间未建立socket连接重启模块*/
    if (WIFI_Param.status == wifi_status_socket_connected) {
        TS_INIT(tsNotConnect);
    } else if (WIFI_Param.status == wifi_status_station_connected)  {
        if (WIFI_Param.ConnectAddress == NULL || WIFI_Param.ConnectPort == 0) {
            TS_INIT(tsNotConnect);
        } else if (TS_IS_OVER(tsNotConnect, WIFI_NOT_CONNECT_TIMEOUT * 1000)) {
            TS_INIT(tsNotConnect);
            WIFI_ReStart();
            DBG_LOG("WIFI socket connect timeout reset.");
        } else {
            WIFI_ScoketConnect(WIFI_Param.ConnectAddress, WIFI_Param.ConnectPort);
            WIFI_GetIPStatus();
        }
    }
    /*设置socket参数*/
    if (WIFI_OPT(WIFI_OPT_SET_SOCKET)) {
        WIFI_OPT_CLEAR(WIFI_OPT_SET_SOCKET);
        if (WIFI_Param.status == wifi_status_socket_connected) {
            WIFI_SocketClose();
            WIFI_GetIPStatus();
        }
    }
    /*重启管理*/
    if (WIFI_OPT(WIFI_OPT_RESET) || WIFI_Param.ErrorCount > 5) {
        WIFI_OPT_CLEAR(WIFI_OPT_RESET);
        WIFI_Param.status = wifi_status_poweroff;
        WIFI_PWR_OFF();
        osDelay(200);
    }
    /*socket未使用时关机省电*/
#if WIFI_POWER_SAVE_EN > 0
    static uint32_t tsPowerSave = 0;

    if (WIFI_Param.ConnectAddress != NULL) {
        TS_INIT(tsPowerSave);
    } else if (TS_IS_OVER(tsPowerSave, WIFI_POWER_SAVE_TIME * 1000) && WIFI_Param.status != wifi_status_poweroff) {
        DBG_LOG("WIFI socket not used, power save.");
        TS_INIT(tsPowerSave);
        WIFI_SetOnOff(FALSE);
    }
#endif
}

/**
 * WIFI socket发送处理
 */
static void WIFI_Socket_SendProc(void)
{
    osEvent evt;
    uint16_t len = 0;
    uint8_t *p = NULL;

    if (WIFI_Param.status == wifi_status_socket_connected) {
        evt = osMessageGet(WIFI_TCPIP_SendQId, 2);
        if (evt.status == osEventMessage) {
            p = evt.value.p;
            len = *(uint16_t *)(p);
            p += 2;
            if (len > 0) {
                if(WIFI_Socket_Send(p, len) == TRUE){                
                    DBG_LOG("Wifi socket send ok len, return:%d", len);
                }
                else{
                    DBG_LOG("Wifi socket send faile");
                }
                vPortFree(evt.value.p);
            }
            
        }
    }
}

/**
 * WIFI 事件处理
 */
static void WIFI_EventProc(void)
{
    char *p = NULL, *pbuf = NULL;
    uint16_t len = 0;
    static uint32_t ts = 0;

    len = UART_DataSize(WIFI_UART_PORT);
    if (len == 0) {
        if (TS_IS_OVER(ts, 10000)) {
            TS_INIT(ts);
            WIFI_GetIPStatus();
        }
        return;
    }
    if (len > 0 && (UART_QueryByte(WIFI_UART_PORT, len - 1) == '\n'
                    && UART_QueryByte(WIFI_UART_PORT, len - 2) == '\r' && UART_GetDataIdleTicks(WIFI_UART_PORT) >= 20)
        || UART_GetDataIdleTicks(WIFI_UART_PORT) >= WIFI_UART_REFRESH_TICK) {

        if (len >= (WIFI_RECEIVE_MAX_SIZE - 1)) {
            len = WIFI_RECEIVE_MAX_SIZE - 1;
        }
        pbuf = MMEMORY_ALLOC(len + 1);
        if (pbuf == NULL) {
            return;
        }

        len = UART_ReadData(WIFI_UART_PORT, (uint8_t *)pbuf, len);
        *(pbuf + len) = 0;
        //DBG_LOG("WIFI_UART_PORT:%s", pbuf);//
        p = WIFI_Scoket_ReceiveProc(pbuf);
        if (p == NULL) {
            p = pbuf;
        }

        /*连接状态更新*/
        if (strstr(p, "CLOSED") || strstr(p, "CONNECT OK") || strstr(p, "WIFI GOT IP")
            || strstr(p, "WIFI DISCONNECT")) {
            WIFI_GetIPStatus();
        }
        MMEMORY_FREE(pbuf);
    }

}

/**
 * WIFI模块读状态
 */
static void WIFI_GetIPStatus(void)
{
    uint8_t i = 0;
    char *p = NULL;

    WIFI_SEND_AT("CIPSTATUS");
    p = WIFI_WAIT_TOKEN("STATUS:", 1000);
    if (p != NULL) {
        while (*p && *p++ != ':');
        i = uatoi(p);
        if (i == 3) {
            WIFI_Param.status = wifi_status_socket_connected;
        } else if (i == 2 || i == 4) {
            WIFI_Param.status = wifi_status_station_connected;
        } else if (WIFI_Param.status != wifi_status_elink) {
            WIFI_Param.status = wifi_status_station_notconnected;
        }
        WIFI_WAIT_ACK("OK", 1000);
    }
}

/**
 * WIFI Socket接收到的数据处理
 */
static char* WIFI_Scoket_ReceiveProc(char *data)
{
    char *p = data, *pret = NULL;
    uint16_t len = 0;

    p = strstr(data, "IPD,");
    while (p != NULL) {
        while (*p && *p++ != ',');
        len = uatoi(p);
        while (*p && *p++ != ':');
#if WIFI_CMD_EN > 0
        CMD_NewData(WIFI_Pipe, (uint8_t *)p, len);
#endif

        if (WIFI_Param.callback != NULL) {
            //DBG_LOG("test wifi rec:%s", p);//
            //DBG_LOG("test wifi rec:%u", len);//
            WIFI_Param.callback((uint8_t *)p, len);
        }
        p = p + len;
        pret = p;
        p =  strstr(p, "IPD,");
    }
    return pret;
}

/**
 * WIFI工作模式初始化
 */
static void WIFI_ModuleInit(void)
{
    char *p = NULL;
    //UART_SetRemapping(4, 1);
    WIFI_RST_OFF();
    WIFI_PWR_ON();
    WIFI_Param.status = wifi_status_poweron;

    WIFI_WAIT_ACK("ready", 300);
    WIFI_SEND_DATA("AT\r\n", 4);
    if (WIFI_WAIT_ACK("OK", 2000)) {
        /*仅用AT模式*/
        WIFI_SEND_AT("CIPMODE=0");
        WIFI_WAIT_ACK("OK", 1000);

        /*配置为station模式*/
        WIFI_SEND_AT("CWMODE?");
        p = WIFI_WAIT_TOKEN("+CWMODE:", 1000);
        if (p != NULL) {
            while (*p && *p++ != ':');
            if (*p != '1') {
                WIFI_SEND_AT("CWMODE=1");
                WIFI_WAIT_ACK("OK", 1000);
            }
        }
        /*开启DHCP*/
        WIFI_SEND_AT("CWMDHCP_CUR=1,1");
        WIFI_WAIT_ACK("OK", 1000);

        /*单socket*/
        WIFI_SEND_AT("CIPMUX=0");
        WIFI_WAIT_ACK("OK", 1000);

        /*开启自动连接*/
        WIFI_SEND_AT("CWAUTOCONN?");
        p = WIFI_WAIT_TOKEN("+CWAUTOCONN:", 1000);
        if (p != NULL) {
            while (*p && *p++ != ':');
            if (*p != '1') {
                WIFI_SEND_AT("CWAUTOCONN=1");
                WIFI_WAIT_ACK("OK", 1000);
            }
        }
        WIFI_GetIPStatus();
    } else {
        WIFI_Param.status = wifi_status_fault;
        WIFI_PWR_OFF();
        DBG_LOG("WIFI module fault.");
    }
}

/**
 * TCPIP发送数据
 * @param data 数据指针
 * @param len  数据长度
 * @return 返回发送结果
 */
static BOOL WIFI_Socket_Send(uint8_t *data, uint16_t len)
{
    if (data != NULL && len > 0) {
        WIFI_AT_PRINTF("CIPSEND=%d", len);
        if (WIFI_WAIT_TOKEN(">", 1000)) {
            WIFI_SEND_DATA(data, len);
            if (WIFI_WAIT_TOKEN("SEND OK", 2000)){
                return TRUE;
            }
            //网络太好时等待 SEND OK  会造成IPD接收失败

        }
    }
    return FALSE;
}

/**
 * WIFI Socket连接
 *
 * @param addr   待连接的IP地址
 * @param port   连接的端口
 * @return 连接成功返回TRUE
 */
static BOOL WIFI_ScoketConnect(char *addr, uint16_t port)
{
    WIFI_AT_PRINTF("CIPSTART=\"TCP\",\"%s\",%d", addr, port);

    if (WIFI_WAIT_ACK("OK", 3000)) {
        return TRUE;
    }
    return FALSE;
}

/**
 * Socket连接关闭
 */
static void WIFI_SocketClose(void)
{
    WIFI_SEND_AT("CIPCLOSE");

    WIFI_WAIT_ACK("OK", 3000);
}

/**
 * 恢复出⼚设置。
 */
static BOOL WIFI_Restore(void)
{
    //WIFI_SEND_AT("CWJAP_DEF=\"abc\",\"0123456789\"");
    WIFI_SEND_AT("CWQAP");
    return (WIFI_WAIT_ACK("OK", 1000) ? TRUE : FALSE);
}
/**
 * 进入配网模式。
 */
static BOOL WIFI_EnterSmartConfig(void)
{
    WIFI_SEND_AT("CWSTARTSMART=3");
    return (WIFI_WAIT_ACK("OK", 1000) ? TRUE : FALSE);
}

/**
 * 进入配网模式。
 */
static BOOL WIFI_ExitSmartConfig(void)
{
    WIFI_SEND_AT("CWSTOPSMART");
    return (WIFI_WAIT_ACK("OK", 1000) ? TRUE : FALSE);
}

/**
 * 获取RSSI的值
 * @return
 *         返回RSSI的值，范围0-100，未连接至AP时返回-1
 */
static int8_t WIFI_GetRSSI(void)
{
    char *p = NULL;
    int8_t temp = 0;
    WIFI_SEND_AT("CWJAP");
    p = WIFI_WAIT_TOKEN("+CWJAP:", 1000);
    if (p != NULL) {
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        temp = atoi(p);
        temp = 100 + temp;
        WIFI_WAIT_ACK("OK", 1000);
        return temp;
    }
    return -1;
}

/**
 * 查找是否有指定的SSID。
 *
 * @param ssid   待搜索的ssid名称
 * @return 返回搜索到的ssid RSSI，未搜索到返回0
 */
static uint8_t WIFI_SeekSSID(char *ssid)
{
    int8_t ret = -1;
    char *p = NULL;

    WIFI_SEND_AT("CWLAP");
    p = WIFI_WAIT_ACK("OK", 5000);
    p = strstr((char *)pRspBuf, "+CWLAP:");
    while (p != NULL && *p) {
        while (*p && *p++ != '\"');
        if (STR_NEQUAL(p, ssid)) {
            while (*p && *p++ != ',');
            ret = atoi(p);
            break;
        }
        p = strstr(p, "+CWLAP:");
    }
    return (100 + ret);
}

/**
 * WIFI等待AT命令返回
 * @param token 等待的token
 * @param time  等待的最长时间,单位毫秒
 * @return 返回等待的token,超时返回NULL
 */
static char* WIFI_WaitATRsp(char *token, uint32_t time)
{
    uint16_t len = 0;
    uint32_t ts = 0;
    char *psearch = NULL;

    ts = HAL_GetTick();
    while (HAL_GetTick() - ts <= time) {
        len = UART_DataSize(WIFI_UART_PORT);
        if (len > 0 && UART_GetDataIdleTicks(WIFI_UART_PORT) >= 20) {
            /*避免未读出的语句影响后面的指令*/
            //psearch = (char *)SearchMemData(pRspBuf, (uint8_t *)token, len, strlen(token));
            uint8_t toke = 0;
            for(uint16_t i = 0; i < len; i++){

                toke = UART_QueryByte(WIFI_UART_PORT, i);
                if((*token == '>' && toke == '>') || toke == '\n'){
                    len = i + 1;
                    break;
                }
            }
            if (toke == '\n'
                || *token == '>'
                || UART_GetDataIdleTicks(WIFI_UART_PORT) >= WIFI_UART_REFRESH_TICK) {
                if (len >= (WIFI_RECEIVE_MAX_SIZE - 1)) {
                    len = WIFI_RECEIVE_MAX_SIZE - 1;
                }
                if (pRspBuf != NULL) {
                    MMEMORY_FREE(pRspBuf);
                    pRspBuf = NULL;
                }

                pRspBuf = MMEMORY_ALLOC(len + 1);
                if (pRspBuf != NULL) {
                    len = UART_ReadData(WIFI_UART_PORT, pRspBuf, len);
                    pRspBuf[len] = '\0';
                    psearch = (char *)SearchMemData(pRspBuf, (uint8_t *)token, len, strlen(token));

                    if (psearch != NULL || strstr((char *)pRspBuf, "ERROR")) {
                        break;
                    }

                }
            }
        }
        osDelay(1);
    }
    if (psearch == NULL) {
        WIFI_Param.ErrorCount++;
    } else {
        WIFI_Param.ErrorCount = 0;
    }
    return psearch;

}

/**
 * WIFI调试命令
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static void WIFI_Console(int argc, char *argv[])
{
    argv++;
    argc--;
    if (strcmp(*argv, "power") == 0) {
        if (strcmp(argv[1], "on") == 0) {
            WIFI_SetOnOff(TRUE);
        } else if (strcmp(argv[1], "off") == 0) {
            WIFI_SetOnOff(FALSE);
        }
        DBG_LOG("WIFI power:%s", argv[1]);
    }  else if (strcmp(*argv, "status") == 0) {
        DBG_LOG("WIFI status:%d", (int)WIFI_Param.status);
    } else if (strcmp(*argv, "rssi") == 0) {
        DBG_LOG("WIFI rssi:%d", WIFI_ReadRSSI());
    } else if (strcmp(*argv, "test") == 0) {
        argv++;
        argc--;
        osMutexWait(WIFI_MutexId, osWaitForever);

        if (strcmp(*argv, "power") == 0) {
            if (strcmp(argv[1], "on") == 0) {
                WIFI_PWR_ON();
            } else if (strcmp(argv[1], "off") == 0) {
                WIFI_PWR_OFF();
            }
            DBG_LOG("WIFI test power:%s", argv[1]);
        } else if (strcmp(*argv, "reset") == 0) {
            if (strcmp(argv[1], "on") == 0) {
                WIFI_RST_ON();
            } else if (strcmp(argv[1], "off") == 0) {
                WIFI_RST_OFF();
            }
            DBG_LOG("WIFI test reset pin:%s", argv[1]);
        } else if (strcmp(*argv, "at") == 0) {
            WIFI_SEND_AT(argv[1]);
            DBG_LOG("wifi test send AT :%s.", argv[1]);
        } else if (strcmp(*argv, "init") == 0) {
            WIFI_ModuleInit();
            DBG_LOG("wifi test module init.");
        } else if (strcmp(*argv, "rssi") == 0) {
            DBG_LOG("wifi test rssi:%d.", WIFI_GetRSSI());
        }  else if (strcmp(*argv, "socketsend") == 0) {
            WIFI_Socket_Send((uint8_t *)argv[1], strlen(argv[1]));
            DBG_LOG("wifi test socket send OK.");
        } else if (strcmp(*argv, "station") == 0) {
            WIFI_SetStation(argv[1], argv[2]);
            DBG_LOG("WIFI test station set OK.");
        } else if (strcmp(*argv, "seekssid") == 0) {
            DBG_LOG("WIFI test seek SSID:%s ret:%d.", argv[1], WIFI_SeekSSID(argv[1]));
        } else if (strcmp(*argv, "entersmart") == 0) {
            WIFI_EnterSmartConfig();
            DBG_LOG("WIFI test enter smart config");
        } 
        else if (strcmp(*argv, "wifirestore") == 0) {
            TS_INIT(tsElink);
            TS_INIT(tsStation);
            WIFI_Restore();
            WIFI_ExitSmartConfig();
            WIFI_Param.status = wifi_status_station_notconnected;           
            DBG_LOG("WIFI restore");
        }else if (strcmp(*argv, "exitsmart") == 0) {
            WIFI_ExitSmartConfig();
            DBG_LOG("WIFI test exit smart config");
        }
        osMutexRelease(WIFI_MutexId);
    }
}
