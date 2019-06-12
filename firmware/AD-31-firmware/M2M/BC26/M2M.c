/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file M2M.c
 * @version V1.0
 * @date 2016.4.1
 * @brief M2M驱动函数文件,适用于SIM800L.
 *
 * *********************************************************************
 * @note
 * 2016.12.18 增加socket接收回调.
 *
 * *********************************************************************
 * @author 宋阳
 */



/* Includes ------------------------------------------------------------------*/
#include "user_comm.h"

/* Private typedef -----------------------------------------------------------*/
static const char *ipRetArray[] = {
    "IP INITIAL",
    "IP START",
    "IP CONFIG",
    "IP M2MACT",
    "IP STATUS",
    "TCP CONNECTING",
    "CONNECT OK",
    "TCP CLOSING",
    "TCP CLOSED",
    "PDP DEACT",
    NULL
};

/*TCPIP连接状态*/
typedef enum {
    ip_status_INITIAL = 0,
    ip_status_CONNECTING,
    ip_status_OK,
    ip_status_CLOSING,
    ip_status_START,
    ip_status_CONFIG,
    ip_status_M2MACT,
    ip_status_STATUS,
    ip_status_CLOSED,
    PDP_DECT
} IP_Status_t;

typedef struct
{
    char       *APN;
    char       *APN_User;
    char       *APN_Pwd;
    char       *ConnectAddress;
    uint16_t    ConnectPort;
    Sock_RecCBFun callback;
    uint8_t     rssi;
    uint16_t    ErrorCount;
    uint16_t    ConnectFailCount;
    IP_Status_t IP_Status;
    M2M_Status_t status;
    BOOL        powerOnOff;
} M2M_Param_t;


/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/
#define M2M_PWR_ON()                   HAL_GPIO_WritePin(M2M_POWER_EN_GPIO_Port, M2M_POWER_EN_Pin, GPIO_PIN_SET) //IO_H有被优化的可能IO_H(M2M_POWER_EN)
#define M2M_PWR_OFF()                  HAL_GPIO_WritePin(M2M_POWER_EN_GPIO_Port, M2M_POWER_EN_Pin, GPIO_PIN_RESET)

#define M2M_KEY_ON()                   HAL_GPIO_WritePin(M2M_POWERKEY_GPIO_Port, M2M_POWERKEY_Pin, GPIO_PIN_RESET)
#define M2M_KEY_OFF()                  HAL_GPIO_WritePin(M2M_POWERKEY_GPIO_Port, M2M_POWERKEY_Pin, GPIO_PIN_RESET)
#define M2M_LED_ON()                   IO_H(LED_M2M)
#define M2M_LED_OFF()                  IO_L(LED_M2M)

#define M2M_SEND_DATA(dat, len)        UART_SendData(M2M_UART_PORT, dat, len)
#define M2M_SEND_AT(cmd)               UART_Printf(M2M_UART_PORT, "AT+%s\r\n", cmd)
#define M2M_AT_PRINTF(format, ...)     UART_Printf(M2M_UART_PORT, "AT+"format"\r\n", ##__VA_ARGS__)

#define M2M_WAIT_ACK(token, timeout)   WaitATRsp(token"\r\n", timeout)
#define M2M_WAIT_TOKEN(token, timeout) WaitATRsp(token, timeout)

#define M2M_OPT_SET(opt)               do {MCPU_ENTER_CRITICAL(); MASK_SET(M2M_Opt, opt); MCPU_EXIT_CRITICAL();} while(0)
#define M2M_OPT_CLEAR(opt)             do {MCPU_ENTER_CRITICAL(); MASK_CLEAR(M2M_Opt, opt); MCPU_EXIT_CRITICAL();} while(0)
#define M2M_OPT(opt)                   IS_MASK_SET(M2M_Opt, opt)

/* Private variables ---------------------------------------------------------*/
static osMessageQId M2M_TCPIP_SendQId;
static M2M_Param_t M2M_Param;
static uint32_t M2M_Opt;
#if M2M_CMD_EN > 0
static uint8_t CMD_Pipe = 0;
#endif

static osMutexId M2M_MutexId;

static uint8_t *pRspBuf = NULL;

/* Private function prototypes -----------------------------------------------*/
void M2M_Task(void const *argument);
static void M2M_ManagerPoll(void);
static void M2M_Intercept_Proc(void);
static void M2M_TCPIP_ReceiveProc(char *pReceive);
static void M2M_TCPIP_SendProc(void);
static BOOL M2M_ModuleInit(void);
static BOOL M2M_ModulePowerOn(void);
static BOOL M2M_ModulePowerOff(void);
static BOOL ConnectShut(void);
static BOOL ConnectClose(void);
static BOOL ConnectStart(uint8_t socketid, char *addr, uint16_t port);
static uint8_t GetRSSI(void);
static IP_Status_t GetIPStatus(void);
static BOOL GetNetWorkStatus(void);
static BOOL ReadPhoneNum(char *num);
static uint32_t HttpGet(char *url, char *readbuf, uint32_t buflen);
static uint16_t TCPIP_Send(uint8_t *data, uint16_t len);
static uint32_t FTP_StartGet(char *server, char *user, char *pwd, char *filenme);
static BOOL FTP_GetData(uint8_t *buf, uint16_t *getlen);
static char* WaitATRsp(char *token, uint16_t time);
static void M2M_Console(int argc, char *argv[]);

/* Exported functions --------------------------------------------------------*/
/**
 * M2M驱动初始化
 */
void M2M_Init(void)
{
    osMutexDef(M2M);
    M2M_MutexId = osMutexCreate(osMutex(M2M));

    osMessageQDef(TCPIP_SendQ, M2M_SEND_Q_SIZE, void *);
    M2M_TCPIP_SendQId = osMessageCreate(osMessageQ(TCPIP_SendQ), NULL);

    osThreadDef(M2M, M2M_Task, M2M_TASK_PRIO, 0, M2M_TASK_STK_SIZE);
    osThreadCreate(osThread(M2M), NULL);

    CMD_ENT_DEF(M2M, M2M_Console);
    Cmd_AddEntrance(CMD_ENT(M2M));

#if M2M_CMD_EN > 0
    CMD_Pipe = CMD_Pipe_Register((CMD_SendFun)M2M_SocketSendData);
    DBG_LOG("M2M CMD pipe is %d.", CMD_Pipe);
#endif

    DBG_LOG("M2M Init");
}

/**
 * M2M任务
 * @param argument 初始化参数
 */
void M2M_Task(void const *argument)
{
    TWDT_DEF(M2MTask, 60000);
    TWDT_ADD(M2MTask);
    TWDT_CLEAR(M2MTask);

    DBG_LOG("M2M task start.");
    while (1) {
        osDelay(5);
        TWDT_CLEAR(M2MTask);
        if (osMutexWait(M2M_MutexId, 1000) == osOK) {
            M2M_ManagerPoll();
            M2M_Intercept_Proc();
            M2M_TCPIP_SendProc();

            if (pRspBuf != NULL) {
                MMEMORY_FREE(pRspBuf);
                pRspBuf = NULL;
            }
            osMutexRelease(M2M_MutexId);
        }
    }
}

/**
 * M2M模块重启
 */
void M2M_ReStart(void)
{
    if (!M2M_OPT(M2M_OPT_RESET)) {
        M2M_OPT_SET(M2M_OPT_RESET);
    }
}

/**
 * M2M模块开关机
 */
void M2M_SetOnOff(BOOL onoff)
{
    if (M2M_Param.powerOnOff != onoff) {
        MCPU_ENTER_CRITICAL();
        M2M_Param.powerOnOff = onoff;
        MCPU_EXIT_CRITICAL();

        if (onoff == FALSE) {
            M2M_OPT_SET(M2M_OPT_RESET);
        }
    }
}

/**
 * 读模块的状态
 * @return 返回模块的状态
 */
M2M_Status_t M2M_ReadStatus(void)
{
    return M2M_Param.status;
}

/**
 * M2M socket 发送数据
 * @param data 数据指针
 * @param len  数据的长度
 * @return 返回发送结果
 */
int16_t M2M_SocketSendData(uint8_t *data, uint16_t len)
{
    osStatus res = osOK;
    uint8_t *pBuf = NULL, *p = NULL;
    if (len > M2M_SEND_MAX_SIZE) {
        len = M2M_SEND_MAX_SIZE;
    }
    if (M2M_Param.IP_Status == ip_status_OK) {
        pBuf = MMEMORY_ALLOC(len + 2);
        if (pBuf != NULL) {
            p = pBuf;
            *(uint16_t *)p = len;
            p += 2;
            memcpy(p, data, len);
            res = osMessagePut(M2M_TCPIP_SendQId, (uint32_t)pBuf, 1000);
            if (res == osOK) {
                return len;
            } else {
                DBG_LOG("M2M socket Send Q fault:%d", (int)res);
                MMEMORY_FREE(pBuf);
                return 0;
            }
        }
    }
    return -1;
}

/**
 * 读M2M模块的RSSI
 * @return 返回RSSI的值
 */
uint8_t M2M_ReadRSSI(void)
{
    uint8_t csq = 0;

    if (osMutexWait(M2M_MutexId, 100) == osOK) {
        csq = GetRSSI();
        M2M_Param.rssi = csq;
        osMutexRelease(M2M_MutexId);
    } else {
        csq = M2M_Param.rssi;
    }
    return csq;
}

/**
 * 读手机SIM卡电话号码
 * @param num  号码返回的指针
 * @return 返回读出结果
 */
BOOL M2M_ReadPhoneNum(char *num)
{
    BOOL ret = FALSE;

    if (num != NULL && osMutexWait(M2M_MutexId, 100) == osOK) {
        ret = ReadPhoneNum(num);
        osMutexRelease(M2M_MutexId);
    }
    return ret;
}

/**
 * HTTP get
 * @param url     链接地址
 * @param getbuf 读出的缓存
 * @param buflen  缓存的长度
 * @return 返回get结果
 */
uint32_t M2M_HTTP_Get(char *url, char *getbuf, uint32_t buflen)
{
    uint32_t ret = FALSE;
    if (M2M_Param.status == M2M_status_online && url != NULL) {
        osMutexWait(M2M_MutexId, osWaitForever);
        ret = HttpGet(url, getbuf, buflen);
        osMutexRelease(M2M_MutexId);
    }

    return ret;
}





/**
 * 获取M2M的连接状态
 * @retur  模块故障返回-1，
 *         无连接返回0,已连接返回1.
 */
int8_t M2M_IsSocketConnect(void)
{
    int8_t ret = -1;

    if (M2M_Param.status != M2M_status_fault && M2M_Param.status != M2M_status_nocard) {
        ret = 0;
    }

    if (M2M_Param.status == M2M_status_online
        && M2M_Param.IP_Status == ip_status_OK
        && !M2M_OPT(M2M_OPT_SET_SOCKET)) {
        ret = 1;
    }
    return ret;
}

/**
 * M2M模块设置socket参数
 * @param server 服务器的IP地址或者域名名
 * @param port   服务器的端口号
 */
void M2M_SetSocketParam(char *server, uint16_t port, Sock_RecCBFun callback)
{
    if (server != M2M_Param.ConnectAddress || port != M2M_Param.ConnectPort || callback != M2M_Param.callback) {
        MCPU_ENTER_CRITICAL();
        M2M_Param.ConnectAddress = server;
        M2M_Param.ConnectPort = port;
        M2M_Param.callback = callback;
        M2M_Opt |= M2M_OPT_SET_SOCKET;
        MCPU_EXIT_CRITICAL();
    }
    if (server != NULL && port > 0) {
        /*M2M开机*/
        M2M_SetOnOff(TRUE);
    }
}

/* Private function prototypes ----------------------------------------------*/
/**
 * M2M管理轮询.
 */
static void M2M_ManagerPoll(void)
{
    static uint32_t tsNet = 0, tsConnect = 0, tsStatus = 0;

    /*错误重启管理*/
    if (M2M_Param.ErrorCount > 10 || M2M_Param.ConnectFailCount > 10 || M2M_OPT(M2M_OPT_RESET)) {
        M2M_OPT_CLEAR(M2M_OPT_RESET);
        M2M_Param.ErrorCount = 0;
        M2M_Param.ConnectFailCount = 0;
        M2M_ModulePowerOff();
        M2M_Param.status = M2M_status_poweroff;
    }

    /*自动开机与状态管理*/
    switch (M2M_Param.status) {
    case M2M_status_poweroff:
        if (M2M_Param.powerOnOff) {
            M2M_ModulePowerOn();
            DBG_LOG("M2M module power on init.");
            DBG_LOG("test1");
        }
        break;
    case M2M_status_poweron:
        M2M_Param.status = (GetNetWorkStatus() == TRUE) ? M2M_status_online : M2M_status_nonet;
        M2M_Param.rssi =  GetRSSI();
        break;
    case M2M_status_nonet:
        if (TS_IS_OVER(tsNet, 30000)) {
            TS_INIT(tsNet);
            if (GetNetWorkStatus() == FALSE) {
                M2M_Param.ConnectFailCount++;
            } else {
                M2M_Param.status = M2M_status_online;
            }
        }
        break;
    default:
        break;
    }
    if (M2M_Param.status == M2M_status_online) {
        /*设置参数*/
        if (M2M_OPT(M2M_OPT_SET_SOCKET)) {
            if (M2M_Param.IP_Status != ip_status_CLOSED && M2M_Param.IP_Status != ip_status_INITIAL) {
                M2M_Param.IP_Status = ip_status_CLOSED;
                M2M_OPT_CLEAR(M2M_OPT_SET_SOCKET);
                ConnectClose();
            } else {
                M2M_OPT_CLEAR(M2M_OPT_SET_SOCKET);
            }
        }
        
        /*维持TCP链路*/
        switch (M2M_Param.IP_Status) {
        case ip_status_CLOSING:{
            ConnectClose();
            //ConnectShut();
            M2M_Param.IP_Status = ip_status_INITIAL;
        }           
        case ip_status_CLOSED:
        case ip_status_INITIAL:
            TS_INIT(tsConnect);
            if (M2M_Param.ConnectAddress != NULL && M2M_Param.ConnectPort > 0) {
                ConnectStart(0, M2M_Param.ConnectAddress, M2M_Param.ConnectPort);
                M2M_Param.IP_Status = GetIPStatus();
            }
            break;
        case ip_status_STATUS:
        case PDP_DECT:
            ConnectShut();
            M2M_Param.IP_Status = ip_status_INITIAL;
            break;
        case ip_status_OK:
            if (TS_IS_OVER(tsStatus, 10000)) {
                TS_INIT(tsStatus);
                M2M_Param.rssi =  GetRSSI();
                M2M_Param.IP_Status = GetIPStatus();
            }
            break;
        default:
            /*20秒未成功连接重连*/
            if (TS_IS_OVER(tsConnect, 20000)) {
                ConnectShut();
                M2M_Param.IP_Status = ip_status_CLOSED;
            }
            if (TS_IS_OVER(tsStatus, 3000)) {
                TS_INIT(tsStatus);
                M2M_Param.IP_Status = GetIPStatus();
            }
            break;
        }
    }
    /*M2M网络离线时复位ip连接状态*/
    else {
        M2M_Param.IP_Status = ip_status_INITIAL;
    }

    /*socket未使用时关机省电*/
#if M2M_POWER_SAVE_EN > 0
    static uint32_t tsPowerSave = 0;

    if (M2M_Param.ConnectAddress != NULL) {
        TS_INIT(tsPowerSave);
    } else if (TS_IS_OVER(tsPowerSave, M2M_POWER_SAVE_TIME * 1000) && M2M_Param.status != M2M_status_poweroff) {
        DBG_LOG("M2M socket not used, power save.");
        TS_INIT(tsPowerSave);
        M2M_SetOnOff(FALSE);
    }
#endif
}

/**
 * M2M 串口数据监听处理
 */
static void M2M_Intercept_Proc(void)
{
    char *p = NULL, *pbuf = NULL;
    uint16_t len = 0;

    len = UART_DataSize(M2M_UART_PORT);
    if (len == 0) {
        return;
    }
    if ((UART_QueryByte(M2M_UART_PORT, len - 1) == '\n'
         && UART_QueryByte(M2M_UART_PORT, len - 2) == '\r' && UART_GetDataIdleTicks(M2M_UART_PORT) >= 20)
        || UART_GetDataIdleTicks(M2M_UART_PORT) >= M2M_UART_REFRESH_TICK) {

        if (len >= (M2M_RECEIVE_MAX_SIZE - 1)) {
            len = M2M_RECEIVE_MAX_SIZE - 1;
        }
        pbuf = MMEMORY_ALLOC(len + 1);
        if (pbuf != NULL) {
            len = UART_ReadData(M2M_UART_PORT, (uint8_t *)pbuf, len);
            *(pbuf + len) = 0;
            p = (char *)pbuf;
            /*连接状态更新*/
            if (strstr(p, "+QIOPEN: 0,0")) {
                M2M_Param.IP_Status = GetIPStatus();
            }
            if(strstr(p, "+QIURC: \"closed\",")){
                M2M_Param.IP_Status = ip_status_CLOSED;
            }
            /*网络状态更新*/
            char* p1 = NULL;
            p1 = strstr(p, "+CEREG: ");
            if (p1) {
                uint8_t net = 0;
                //while (*p1 && (*p1++) != ',');
                net = uatoi(p1 + 8);
                //(net == 1 || net == 5) ? TRUE : FALSE;
                M2M_Param.status = ((net == 1 || net == 5) ? M2M_status_online : M2M_status_nonet);
                M2M_Param.rssi =  GetRSSI();
            }
            M2M_TCPIP_ReceiveProc(pbuf);
            MMEMORY_FREE(pbuf);
        }
    }
}

/**
 * TCPIP数据接收收处理
 * @param pReceive 接收到的数据的指针
 */
static void M2M_TCPIP_ReceiveProc(char *pReceive)
{
    uint16_t len = 0;
    char *p = NULL;

    p = strstr(pReceive, "+QIURC: \"recv\",0,");
    while (p != NULL) {
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        len = uatoi(p);
        while (*p && *p++ != '\n');
        HexStrToByte(p, (uint8_t *)p, len << 1);
#if M2M_CMD_EN > 0
        CMD_NewData(CMD_Pipe, (uint8_t *)p, len);
#endif
        if (M2M_Param.callback != NULL) {
            M2M_Param.callback((uint8_t *)p, len);
        }

        p = p + len + 1;
        p =  strstr(p, "+QIURC: \"recv\",0,");
    }
}

/**
 * TCPIP数据发送处理
 */
static void M2M_TCPIP_SendProc(void)
{
    osEvent evt;
    uint16_t len = 0;
    uint8_t *p = NULL;

    if (M2M_Param.IP_Status == ip_status_OK) {
        evt = osMessageGet(M2M_TCPIP_SendQId, 2);
        if (evt.status == osEventMessage) {
            p = evt.value.p;
            len = *(uint16_t *)(p);
            p += 2;

            if (len > 0) {
                if (TCPIP_Send(p, len) == FALSE) {
                    M2M_Param.IP_Status = GetIPStatus();
                }
                MMEMORY_FREE(evt.value.p);
            }
        }
    }
}
/**
 * TCPIP发送数据
 * @param data 数据指针
 * @param len  数据长度
 * @return 返回发送结果
 */
static uint16_t TCPIP_Send(uint8_t *data, uint16_t len)
{
    uint16_t sent = 0;
    char *p = NULL;
    char *hexdata = NULL;
    if (data != NULL && len > 0) {
        hexdata = pvPortMalloc((len << 1));  
        if(hexdata != NULL){
            ByteToHexStr(data, hexdata, len);
            char tmp[25];
            sprintf(tmp, "AT+QISENDEX=0,%d,", len);
            M2M_SEND_DATA((uint8_t*)tmp,strlen(tmp));        
            M2M_SEND_DATA((uint8_t*)hexdata, (len << 1));
            vPortFree(hexdata); 
            DBG_LOG("hexdata len: %d \r\n",(len << 1));
            M2M_SEND_DATA("\r\n", 2);                    
            p = M2M_WAIT_TOKEN("SEND OK", 10000);
            if(p != NULL){
                sent = len;
            }   
                        
        }  
        
    }
    return sent;
}

/**
 * M2M调试命令
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static BOOL M2M_ModuleInit(void)
{
    char *p = NULL;
    BOOL r = FALSE;

    M2M_SEND_DATA("ATE1\r", 5);
    M2M_WAIT_ACK("OK", 100);   
    M2M_SEND_DATA("AT\r", 3);
    M2M_WAIT_ACK("OK", 100);  
    M2M_SEND_DATA("AT\r", 3);
    M2M_WAIT_ACK("OK", 100);
    M2M_SEND_AT("CFUN=1");
    M2M_WAIT_ACK("OK", 100); 
    M2M_SEND_AT("CEREG=2");
    M2M_WAIT_ACK("OK", 100);  
     
    M2M_SEND_AT("CPIN?");
    p = M2M_WAIT_TOKEN("+CPIN:", 200);
    if (p != NULL) {
        if (strstr(p, "READY")) {
            r = TRUE;
        }
    }
    return r;
}


/**
 * M2M上电开机
 */
static BOOL M2M_ModulePowerOn(void)
{
    BOOL r = FALSE;

    /*掉电延时确保模块开机成功*/
    M2M_KEY_OFF();
    M2M_PWR_OFF();
    osDelay(200);

    UART_SetBaudrate(M2M_UART_PORT, M2M_UART_BDR);
    M2M_PWR_ON();
    M2M_KEY_ON();

    if (M2M_WAIT_ACK("RDY", 10000)) {
        M2M_KEY_OFF();
        if (M2M_ModuleInit()) {
            M2M_Param.status = M2M_status_poweron;
        } else {
            M2M_Param.status = M2M_status_nocard;
        }
    } else {
        M2M_KEY_OFF();
        M2M_SEND_DATA("AT\r", 3);
        M2M_WAIT_ACK("OK", 100);
        M2M_SEND_DATA("AT\r", 3);
        if (M2M_WAIT_ACK("OK", 1000)) {            
            if (M2M_ModuleInit()) {
                M2M_Param.status = M2M_status_poweron;
            } else {
                M2M_Param.status = M2M_status_nocard;
            }
        } else {
            M2M_Param.status = M2M_status_fault;
            M2M_KEY_OFF();
            M2M_PWR_OFF();
        }
    }
    if (M2M_Param.status == M2M_status_nocard) {
        M2M_KEY_OFF();
        M2M_PWR_OFF();
    }
    return r;
}

/**
 * M2M关机关电
 */
static BOOL M2M_ModulePowerOff(void)
{
    BOOL r = FALSE;

    M2M_KEY_ON();
    if (M2M_WAIT_ACK("OK", 3000)) {
        r = TRUE;
    }
    M2M_KEY_OFF();
    M2M_PWR_OFF();
    return r;
}

/**
 * M2M关闭连接与PDP场景
 * @return 返回关闭结果
 */
static BOOL ConnectShut(void)
{
    M2M_SEND_AT("CGACT=0,1");
    return (M2M_WAIT_ACK("NO CARRIER", 10000) || M2M_WAIT_ACK("OK", 10000)) ? TRUE : FALSE;
}
/**
 * M2M关闭连接与PDP场景
 * @return 返回关闭结果
 */
static BOOL PdpOpen(void)
{
    M2M_SEND_AT("CGACT=1,1");
    return M2M_WAIT_ACK("OK", 10000) ? TRUE : FALSE;
}
/**
 * M2M关闭连接
 * @return 返回关闭结果
 */
static BOOL ConnectClose(void)
{
    M2M_SEND_AT("QICLOSE=0");
    return M2M_WAIT_ACK("CLOSE OK", 10000) ? TRUE : FALSE;
}

/**
 * M2M建立连接
 * @param addr 连接的IP地址或者域名
 * @param port 连接的端口号
 * @return 返回连接结果
 */
static BOOL ConnectStart(uint8_t socketid, char *addr, uint16_t port)
{
    BOOL r = FALSE;
    //ConnectOpen();
    M2M_WAIT_ACK("OK", 1000);

    M2M_AT_PRINTF("QIOPEN=1,0,\"TCP\",\"%s\",\"%d\",0,0,0", addr, port);
    if (M2M_WAIT_ACK("+QIOPEN: 0,0", 10000)) {
        r = TRUE;
        M2M_SEND_AT("QISWTMD=0,1");
        M2M_WAIT_ACK("OK", 100); 
        M2M_SEND_AT("QICFG=\"showlength\",1");
        M2M_WAIT_ACK("OK", 100); 
        M2M_SEND_AT("QICFG=\"dataformat\",1,1");
        M2M_WAIT_ACK("OK", 100);
    }
    
    return  r;
}

/**
 * 获取RSII的值
 * @return 返回RSSI的值
 */
static uint8_t GetRSSI(void)
{
    uint8_t rssi = 0;
    char *p = NULL;

    M2M_SEND_AT("CSQ");
    p = M2M_WAIT_TOKEN("+CSQ:", 1000);

    if (p != NULL) {
        while (*p && !(isdigit(*p))) {
            p++;
        }
        rssi = uatoi(p);
    }
    return rssi;
}

/**
 * 获取RSII的值
 * @return 返回RSSI的值
 */
static IP_Status_t GetIPStatus(void)
{
    uint8_t i = 3;
    char *p = NULL;
    M2M_SEND_AT("QISTATE=0,1");
    p = M2M_WAIT_TOKEN("+QISTATE: ", 10000);
    if (p != NULL) {
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        i = uatoi(p);
    }
    return (IP_Status_t)i;
}

/**
 * 获取网络状态
 * @return 返回网络注册状态
 */
static BOOL GetNetWorkStatus(void)
{
    uint8_t net = 0;
    char *p = NULL;

    M2M_SEND_AT("CEREG?");
    p = M2M_WAIT_TOKEN("+CEREG: ", 1000);

    if (p != NULL) {
        while (*p && *p++ != ',');
        //while (*p && *p++ != ',');
        net = uatoi(p);
    }
    return (net == 1 || net == 5) ? TRUE : FALSE;
}

/**
 * 读手机SIM卡电话号码
 * @param num  号码返回的指针
 * @return 返回读出结果
 */
static BOOL ReadPhoneNum(char *num)
{
    char *p = NULL;
    BOOL ret = FALSE;

    if (num != NULL && osMutexWait(M2M_MutexId, 100) == osOK) {
        M2M_SEND_AT("CNUM");
        p = M2M_WAIT_ACK("+CNUM:", 200);
        if (p !=  NULL) {
            while (*p && *p++ != ',');
            while (*p && *p++ != '\"');
            while (*p && *p != '\"') {
                *num++ = *p++;
            }
        }
        ret = TRUE;
        osMutexRelease(M2M_MutexId);
    }
    return ret;
}

/**
 * HTTP get
 * @param url     链接地址
 * @param readbuf 读出的缓存
 * @param buflen  缓存的长度
 * @return 返回get结果
 */
static uint32_t HttpGet(char *url, char *readbuf, uint32_t buflen)
{
    uint32_t len = 0, ret = 0;
    char *p = NULL;

    M2M_SEND_AT("SAPBR=1,1");
    M2M_WAIT_ACK("OK", 10000);

    M2M_SEND_AT("HTTPINIT");
    if (M2M_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }

    M2M_SEND_AT("HTTPPARA=\"CID\",1");
    if (M2M_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }

    M2M_AT_PRINTF("HTTPPARA=\"URL\",\"%s\"", url);
    if (M2M_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }

    M2M_SEND_AT("HTTPACTION=0");
    p = M2M_WAIT_TOKEN("+HTTPACTION:", 15000);
    if (p != NULL) {
        while (*p && *p++ != ',');
        /*返加200 OK*/
        if (uatoi(p) == 200) {
            M2M_SEND_AT("HTTPREAD");

            p = M2M_WAIT_TOKEN("+HTTPREAD:", 10000);
            if (p != NULL) {
                while (*p && *p++ != ':');
                p++;
                len = uatoi(p);
                if (len > buflen) {
                    len = buflen;
                }
                while (*p && *p++ != '\n');
                memcpy(readbuf, p, len);
                ret = len;
            }
        }
    }
    M2M_SEND_AT("HTTPTERM");
    M2M_WAIT_ACK("OK", 1000);

    M2M_SEND_AT("SAPBR=0,1");
    M2M_WAIT_ACK("OK", 10000);

    return ret;
}


/**
 * 开始FTP下载
 * @param server FTP服务器地址
 * @param user   FTP账号
 * @param pwd    FTP密码
 * @param path   文件路径
 * @return get成功返回文件的长度，否则返回0
 */
static uint32_t FTP_StartGet(char *server, char *user, char *pwd, char *filename)
{
    char *p = NULL;
    uint32_t size = 0;

    M2M_SEND_AT("SAPBR=1,1");
    M2M_WAIT_ACK("OK", 10000);

    M2M_SEND_AT("FTPCID=1");
    if (M2M_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    M2M_AT_PRINTF("FTPSERV=\"%s\"", server);
    if (M2M_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    M2M_AT_PRINTF("FTPUN=\"%s\"", user);
    if (M2M_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    M2M_AT_PRINTF("FTPPW=\"%s\"", pwd);
    if (M2M_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    M2M_AT_PRINTF("FTPGETNAME=\"%s\"", filename);
    if (M2M_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    M2M_AT_PRINTF("FTPGETPATH=\"%s\"", "/");
    if (M2M_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    M2M_SEND_AT("FTPSIZE");
    p = M2M_WAIT_TOKEN("+FTPSIZE:", 15000);
    if (p != NULL) {
        while (*p && *p++ != ',');
        if (*p == '0') {
            while (*p && *p++ != ',');
            size = uatoi(p);
        }
    }
    if (size > 0) {
        M2M_SEND_AT("FTPGET=1");
        M2M_WAIT_ACK("OK", 1000);

        M2M_WAIT_TOKEN("+FTPGET:", 15000);
    }
    return size;
}

/**
 * FTP 读出数据
 * @param buf    数据读存的缓存
 * @param getlen 数据读出的长度指针，传入需读入的长度，传出实际读出的长度
 * @return FTP会话存在时返回TRUE
 */
static BOOL FTP_GetData(uint8_t *buf, uint16_t *getlen)
{
    BOOL ret = FALSE;
    char *p = NULL;
    uint16_t size = 0;

    M2M_AT_PRINTF("FTPGET=2,%d", *getlen);
    p = M2M_WAIT_TOKEN("+FTPGET:", 3000);
    if (p != NULL) {
        while (*p && *p++ != ':');
        p++;
        if (*p == '1') {
            while (*p && *p++ != ',');
            if (*p == '1') {
                ret = TRUE;
            }
        } else if (*p == '2')  {
            while (*p && *p++ != ',');
            size = uatoi(p);
            if (size > 0 && buf != NULL) {
                while (*p && *p++ != '\n');
                memcpy(buf, p, size);
            }
            ret = TRUE;
        }
    }
    *getlen = size;
    if (size == 0 && ret == FALSE) {
        M2M_SEND_AT("SAPBR=0,1");
        M2M_WAIT_ACK("OK", 10000);
    }
    return ret;
}

/**
 * M2M等待AT命令返回
 * @param token 等待的token
 * @param time  等待的最长时间
 * @return 返回等待的token,超时返回NULL
 */
static char* WaitATRsp(char *token, uint16_t time)
{
    uint16_t len = 0;
    uint32_t ts = 0;
    char *psearch = NULL;

    ts = HAL_GetTick();
    while (HAL_GetTick() - ts <= time) {
        len = UART_DataSize(M2M_UART_PORT);
        if (len > 0 && UART_GetDataIdleTicks(M2M_UART_PORT) >= 20) {
            /*避免未读出的语句影响后面的指令*/
            if (UART_QueryByte(M2M_UART_PORT, len - 1) == '\n'
                || *token == '>'
                || UART_GetDataIdleTicks(M2M_UART_PORT) >= M2M_UART_REFRESH_TICK) {
                if (len >= (M2M_RECEIVE_MAX_SIZE - 1)) {
                    len = M2M_RECEIVE_MAX_SIZE - 1;
                }
                if (pRspBuf != NULL) {
                    MMEMORY_FREE(pRspBuf);
                    pRspBuf = NULL;
                }
                pRspBuf = MMEMORY_ALLOC(len + 1);
                if (pRspBuf != NULL) {
                    len = UART_ReadData(M2M_UART_PORT, pRspBuf, len);
                    pRspBuf[len] = '\0';
                    psearch = (char *)SearchMemData(pRspBuf, (uint8_t *)token, len, strlen(token));
                    if (psearch != NULL || strstr((char *)pRspBuf, "ERROR")) {
                        break;
                    }
                }
            }
        }
        osDelay(2);
    }
    if (psearch == NULL) {
        M2M_Param.ErrorCount++;
    } else {
        M2M_Param.ErrorCount = 0;
    }
    return psearch;
}

/**
 * M2M调试命令
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static void M2M_Console(int argc, char *argv[])
{
    char *pssl = NULL;
    uint16_t len = 0;

    argv++;
    argc--;
    if (strcmp(argv[0], "power") == 0) {
        if (strcmp(argv[1], "on") == 0) {
            M2M_SetOnOff(TRUE);
        } else if (strcmp(argv[1], "off") == 0) {
            M2M_SetOnOff(FALSE);
        }
        DBG_LOG("M2M power:%s", argv[1]);
    }  else if (strcmp(*argv, "status") == 0) {
        DBG_LOG("M2M status:%d, IP status:%s.", (int)M2M_Param.status, ipRetArray[(int)M2M_Param.IP_Status]);
    }  else if (strcmp(*argv, "key") == 0) {
        if (strcmp(argv[1], "on") == 0) {
            M2M_KEY_ON();
        } else if (strcmp(argv[1], "off") == 0) {
            M2M_KEY_OFF();
        }
        DBG_LOG("M2M power key:%s", argv[1]);
    } else if (strcmp(argv[0], "phonenum") == 0) {
        char *num = NULL;
        BOOL ret = FALSE;

        num = MMEMORY_ALLOC(16);
        ret = ReadPhoneNum(num);
        if (ret != FALSE) {
            DBG_LOG("M2M read phonenum ret:%s", num);
        } else {
            DBG_LOG("M2M read phonenum fail.");
        }
        MMEMORY_FREE(num);
    } else if (strcmp(*argv, "test") == 0) {
        argv++;
        argc--;

        osMutexWait(M2M_MutexId, osWaitForever);
        if (strcmp(argv[0], "poweron") == 0) {
            DBG_LOG("M2M test power on.");
            M2M_ModulePowerOn();
        } else if (strcmp(argv[0], "poweroff") == 0) {
            DBG_LOG("M2M test power off.");
            M2M_ModulePowerOff();
        } else if (strcmp(argv[0], "csq") == 0) {
            uint8_t rssi = GetRSSI();

            DBG_LOG("M2M test CSQ:%d.", rssi);
        } else if (strcmp(argv[0], "at") == 0) {
            M2M_SEND_AT(argv[1]);
            DBG_LOG("M2M test send AT :%s.", argv[1]);
        } else if (strcmp(argv[0], "send") == 0) {
            M2M_SEND_DATA((uint8_t *)argv[1], strlen(argv[1]));
            DBG_LOG("M2M test send data OK.");
        } else if (strcmp(argv[0], "httpget") == 0) {
            len = uatoi(argv[2]);
            if (len > 0) {
                pssl = MMEMORY_ALLOC(len);
                if (pssl != NULL) {
                    DBG_LOG("M2M test HTTP get start, bufflen:%d.", len);
                    len = HttpGet(argv[1], pssl, len);
                    if (len > 0) {
                        *(pssl + len) = 0;
                        DBG_LOG("test httpget ret:%s", pssl);
                    }
                    MMEMORY_FREE(pssl);
                }
            }
        } else if (strcmp(argv[0], "ftpstart") == 0) {
            DBG_LOG("M2M test FTP get start ret:%d.",
                    (int)FTP_StartGet(argv[1], argv[2], argv[3], argv[4]));
        } else if (strcmp(argv[0], "ftpget") == 0) {
            len = uatoi(argv[1]);
            DBG_LOG("M2M test FTP get data len: %d, ret:%d.",
                    len,
                    (int)FTP_GetData(NULL, &len));
        } else  if (strcmp(argv[0], "power") == 0) {
            if (strcmp(argv[1], "on") == 0) {
                M2M_PWR_ON();
            } else if (strcmp(argv[1], "off") == 0) {
                M2M_PWR_OFF();
            }
            DBG_LOG("M2M test power:%s", argv[1]);
        }
        osMutexRelease(M2M_MutexId);
    }
}
