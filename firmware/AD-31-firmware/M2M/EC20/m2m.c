/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file M2M.c
 * @version V1.0
 * @date 2016.4.1
 * @brief M2M���������ļ�,������M26.
 *
 * *********************************************************************
 * @note
 * 2016.12.18 ����socket���ջص�.
 *
 * *********************************************************************
 * @author ����
 */



/* Includes ------------------------------------------------------------------*/
#include "user_comm.h"
#include "m2m_cfg.h"
/* Private typedef -----------------------------------------------------------*/
/*TCPIP����״̬*/
typedef enum {
    ip_status_initial = 0,
    ip_status_Opening,
    ip_status_Connected,
    ip_status_Listening,
    ip_status_Closing,
    ip_status_Closed
} IP_Status_t;

typedef struct {
    char* APN;
    char* APN_User;
    char* APN_Pwd;
    char* ConnectAddress;
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
#define M2M_PWR_ON()                   HAL_GPIO_WritePin(M2M_POWER_EN_GPIO_Port, M2M_POWER_EN_Pin, GPIO_PIN_SET)
#define M2M_PWR_OFF()                  HAL_GPIO_WritePin(M2M_POWER_EN_GPIO_Port, M2M_POWER_EN_Pin, GPIO_PIN_RESET)

#define M2M_KEY_ON()                   HAL_GPIO_WritePin(M2M_POWERKEY_GPIO_Port, M2M_POWERKEY_Pin, GPIO_PIN_RESET)
#define M2M_KEY_OFF()                  HAL_GPIO_WritePin(M2M_POWERKEY_GPIO_Port, M2M_POWERKEY_Pin, GPIO_PIN_SET)

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

static osMutexId M2M_MutexId;

static uint8_t* pRspBuf = NULL;
#if M2M_CMD_EN > 0
static uint8_t CMD_Pipe = 0;
#endif

/* Private function prototypes -----------------------------------------------*/
void M2M_Task(void const* argument);
static void M2M_ManagerPoll(void);
static void M2M_Intercept_Proc(void);
static void M2M_TCPIP_ReceiveProc(char* pReceive);
static void M2M_TCPIP_SendProc(void);
static BOOL M2M_ModuleInit(void);
static BOOL M2M_ModulePowerOn(void);
static BOOL M2M_ModulePowerOff(void);
static BOOL ConnectShut(void);
static BOOL ConnectClose(void);
static BOOL ConnectStart(char* addr, uint16_t port);
static uint8_t GetRSSI(void);
static IP_Status_t GetIPStatus(void);
static BOOL GetNetWorkStatus(void);
static BOOL ReadPhoneNum(char* num);
static uint16_t TCPIP_Send(uint8_t* data, uint16_t len);
static void TTS_Play(char* text);
static char* WaitATRsp(char* token, uint16_t time);
static void M2M_Console(int argc, char* argv[]);

/* Exported functions --------------------------------------------------------*/
/**
 * M2M������ʼ��
 */
void M2M_Init(void) {
    osMutexDef(M2M);
    M2M_MutexId = osMutexCreate(osMutex(M2M));
    osMessageQDef(TCPIP_SendQ, M2M_SEND_Q_SIZE, void*);
    M2M_TCPIP_SendQId = osMessageCreate(osMessageQ(TCPIP_SendQ), NULL);
    osThreadDef(M2M, M2M_Task, M2M_TASK_PRIO, 0, M2M_TASK_STK_SIZE);
    osThreadCreate(osThread(M2M), NULL);
    CMD_ENT_DEF(ec20, M2M_Console);
    Cmd_AddEntrance(CMD_ENT(ec20));
#if M2M_CMD_EN > 0
    CMD_Pipe = CMD_Pipe_Register((CMD_SendFun)M2M_SocketSendData);
    DBG_LOG("M2M CMD pipe is %d.", CMD_Pipe);
#endif
    DBG_LOG("M2M Init");
}

/**
 * M2M����
 * @param argument ��ʼ������
 */
void M2M_Task(void const* argument) {
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
 * M2Mģ������
 */
void M2M_ReStart(void) {
    if (!M2M_OPT(M2M_OPT_RESET)) {
        M2M_OPT_SET(M2M_OPT_RESET);
    }
}

/**
 * M2Mģ�鿪�ػ�
 */
void M2M_SetOnOff(BOOL onoff) {
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
 * ��ģ���״̬
 * @return ����ģ���״̬
 */
M2M_Status_t M2M_ReadStatus(void) {
    return M2M_Param.status;
}

/**
 * M2M socket ��������
 * @param data ����ָ��
 * @param len  ���ݵĳ���
 * @return ���ط��ͽ��
 */
int16_t M2M_SocketSendData(uint8_t* data, uint16_t len) {
    osStatus res = osOK;
    uint8_t* pBuf = NULL, *p = NULL;
    if (len > M2M_SEND_MAX_SIZE) {
        len = M2M_SEND_MAX_SIZE;
    }
    if (M2M_Param.IP_Status == ip_status_Connected) {
        pBuf = MMEMORY_ALLOC(len + 2);
        if (pBuf != NULL) {
            p = pBuf;
            *(uint16_t*)p = len;
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
 * ��M2Mģ���RSSI
 * @return ����RSSI��ֵ
 */
uint8_t M2M_ReadRSSI(void) {
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
 * ���ֻ�SIM���绰����
 * @param num  ���뷵�ص�ָ��
 * @return ���ض������
 */
BOOL M2M_ReadPhoneNum(char* num) {
    BOOL ret = FALSE;
    if (num != NULL && osMutexWait(M2M_MutexId, 100) == osOK) {
        ret = ReadPhoneNum(num);
        osMutexRelease(M2M_MutexId);
    }
    return ret;
}

/**
 * M2M����TTS
 *
 * @param text   �����ŵ��ı�
 * @return ���ųɹ�����TRUE
 */
BOOL M2M_TTS(char* text) {
    if (text != NULL && M2M_Param.status != M2M_status_poweroff && M2M_Param.status != M2M_status_fault) {
        osMutexWait(M2M_MutexId, osWaitForever);
        TTS_Play(text);
        osMutexRelease(M2M_MutexId);
        return  TRUE;
    }
    return FALSE;
}

/**
 * ��ȡM2M������״̬
 * @retur  ģ����Ϸ���-1��
 *         �����ӷ���0,�����ӷ���1.
 */
int8_t M2M_IsSocketConnect(void) {
    int8_t ret = -1;
    if (M2M_Param.status != M2M_status_fault && M2M_Param.status != M2M_status_nocard) {
        ret = 0;
    }
    if (M2M_Param.status == M2M_status_online
            && M2M_Param.IP_Status == ip_status_Connected
            && !M2M_OPT(M2M_OPT_SET_SOCKET)) {
        ret = 1;
    }
    return ret;
}

/**
 * M2Mģ������socket����
 * @param server ��������IP��ַ����������
 * @param port   �������Ķ˿ں�
 */
void M2M_SetSocketParam(char* server, uint16_t port, Sock_RecCBFun callback) {
    if (server != M2M_Param.ConnectAddress || port != M2M_Param.ConnectPort || callback != M2M_Param.callback) {
        MCPU_ENTER_CRITICAL();
        M2M_Param.ConnectAddress = server;
        M2M_Param.ConnectPort = port;
        M2M_Param.callback = callback;
        M2M_Param.IP_Status = ip_status_Closing;
        M2M_Opt |= M2M_OPT_SET_SOCKET;
        MCPU_EXIT_CRITICAL();
    }
    if (server != NULL && port > 0) {
        /*M2M����*/
        M2M_SetOnOff(TRUE);
    }
}

/* Private function prototypes ----------------------------------------------*/
/**
 * M2M������ѯ.
 */
static void M2M_ManagerPoll(void) {
    static uint32_t tsNet = 0, tsConnect = 0, tsStatus = 0;
    /*������������*/
    if (M2M_Param.ErrorCount > 10 || M2M_Param.ConnectFailCount > 10 || M2M_OPT(M2M_OPT_RESET)) {
        M2M_OPT_CLEAR(M2M_OPT_RESET);
        M2M_Param.ErrorCount = 0;
        M2M_Param.ConnectFailCount = 0;
        M2M_ModulePowerOff();
        M2M_Param.status = M2M_status_poweroff;
    }
    /*�Զ�������״̬����*/
    switch (M2M_Param.status) {
        case M2M_status_poweroff:
            if (M2M_Param.powerOnOff) {
                DBG_LOG("M2M module power on init.");
                M2M_ModulePowerOn();
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
        /*���ò���*/
        if (M2M_OPT(M2M_OPT_SET_SOCKET)) {
            if (M2M_Param.IP_Status != ip_status_Closed && M2M_Param.IP_Status != ip_status_initial) {
                ConnectClose();
                TS_INIT(tsStatus);
                M2M_Param.IP_Status = GetIPStatus();
                M2M_OPT_CLEAR(M2M_OPT_SET_SOCKET);
            } else {
                M2M_OPT_CLEAR(M2M_OPT_SET_SOCKET);
            }
        }
        /*ά��TCP��·*/
        switch (M2M_Param.IP_Status) {
            case ip_status_Closed:
            case ip_status_initial:
                TS_INIT(tsConnect);
                TS_INIT(tsStatus);
                if (M2M_Param.ConnectAddress != NULL && M2M_Param.ConnectPort > 0) {
                    ConnectStart(M2M_Param.ConnectAddress, M2M_Param.ConnectPort);
                    M2M_Param.IP_Status = GetIPStatus();
                }
                break;
            case ip_status_Connected:
            case ip_status_Listening:
                break;
            case ip_status_Closing:
            case ip_status_Opening:
                /*20��δ�ɹ���������*/
                if (TS_IS_OVER(tsConnect, 20000)) {
                    ConnectShut();
                    M2M_Param.IP_Status = ip_status_Closed;
                }
                if (TS_IS_OVER(tsStatus, 3000)) {
                    TS_INIT(tsStatus);
                    M2M_Param.IP_Status = GetIPStatus();
                    if (M2M_Param.IP_Status == ip_status_Closing) {
                        ConnectShut();
                    }
                }
            default:
                break;
        }
    }
    /*GSM��������ʱ��λip����״̬*/
    else {
        M2M_Param.IP_Status = ip_status_initial;
    }
    /*socketδʹ��ʱ�ػ�ʡ��*/
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
 * M2M �������ݼ�������
 */
static void M2M_Intercept_Proc(void) {
    char* p = NULL, *pbuf = NULL;
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
            len = UART_ReadData(M2M_UART_PORT, (uint8_t*)pbuf, len);
            *(pbuf + len) = 0;
            p = (char*)pbuf;
            /*����״̬����*/
            if (strstr(p, "+QIURC: \"closed\"") || strstr(p, "+QIOPEN:")) {
                M2M_Param.IP_Status = GetIPStatus();
            }
            /*����״̬����*/
            if (strstr(p, "+CREG:")) {
                M2M_Param.status = ((GetNetWorkStatus() == TRUE) ? M2M_status_online : M2M_status_nonet);
                M2M_Param.rssi =  GetRSSI();
            }
            M2M_TCPIP_ReceiveProc(pbuf);
            MMEMORY_FREE(pbuf);
        }
    }
}

/**
 * TCPIP���ݽ����մ���
 * @param pReceive ���յ������ݵ�ָ��
 */
static void M2M_TCPIP_ReceiveProc(char* pReceive) {
    uint16_t len = 0;
    char* p = NULL;
    p = strstr(pReceive, "+QIURC: \"recv\"");
    while (p != NULL) {
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        len = uatoi(p);
        while (*p && *p++ != '\n');
        DBG_LOG("test len:%d", len);
#if M2M_CMD_EN > 0
        CMD_NewData(CMD_Pipe, (uint8_t*)p, len);
#endif
        if (M2M_Param.callback != NULL) {
            M2M_Param.callback((uint8_t*)p, len);
        }
        p = p + len + 1;
        p =  strstr(p, "IPD,");
    }
}

/**
 * TCPIP���ݷ��ʹ���
 */
static void M2M_TCPIP_SendProc(void) {
    osEvent evt;
    uint16_t len = 0;
    uint8_t* p = NULL;
    if (M2M_Param.IP_Status == ip_status_Connected) {
        evt = osMessageGet(M2M_TCPIP_SendQId, 2);
        if (evt.status == osEventMessage) {
            p = evt.value.p;
            len = *(uint16_t*)(p);
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
 * M2M��������
 * @param argc ����������
 * @param argv �����б�
 */
static BOOL M2M_ModuleInit(void) {
    char* p = NULL;
    BOOL r = FALSE;
    M2M_SEND_DATA("ATE1\r", 5);
    M2M_WAIT_ACK("OK", 100);
    M2M_SEND_DATA("ATV1\r", 5);
    M2M_WAIT_ACK("OK", 100);
    M2M_WAIT_TOKEN("+CPIN:", 5000);
    M2M_SEND_AT("QSCLK=0");
    M2M_WAIT_ACK("OK", 100);
    M2M_SEND_AT("IFC=0,0");
    M2M_WAIT_ACK("OK", 100);
    M2M_SEND_AT("CREG=2");
    M2M_WAIT_ACK("OK", 100);
    M2M_SEND_AT("QSIDET=128");
    M2M_WAIT_ACK("OK", 100);
    M2M_SEND_AT("CPIN?");
    p = M2M_WAIT_TOKEN("+CPIN:", 1000);
    if (p != NULL) {
        if (strstr(p, "READY")) {
            r = TRUE;
        }
    }
    return r;
}

/**
 * M2M�ϵ翪��
 */
static BOOL M2M_ModulePowerOn(void) {
    BOOL r = FALSE;
    /*������ʱȷ��ģ�鿪���ɹ�*/
    M2M_KEY_OFF();
    M2M_PWR_OFF();
    osDelay(1000);
    UART_SetBaudrate(M2M_UART_PORT, M2M_UART_BDR);
    M2M_PWR_ON();
    M2M_KEY_ON();
    if (M2M_WAIT_ACK("RDY", 15000)) {
        M2M_KEY_OFF();
        if (M2M_ModuleInit()) {
            M2M_Param.status = M2M_status_poweron;
        } else {
            M2M_Param.status = M2M_status_nocard;
        }
    } else {
        M2M_KEY_OFF();
        M2M_SEND_DATA("AT\r\n", 4);
        M2M_WAIT_ACK("OK", 100);
        M2M_SEND_DATA("AT\r\n", 4);
        if (M2M_WAIT_ACK("OK", 1000)) {
            M2M_AT_PRINTF("IPR=%d", M2M_UART_BDR);
            M2M_WAIT_ACK("OK", 100);
            M2M_SEND_DATA("AT&W\r", 5);
            M2M_WAIT_ACK("OK", 200);
            if (M2M_ModuleInit()) {
                M2M_Param.status = M2M_status_poweron;
            } else {
                M2M_Param.status = M2M_status_nocard;
            }
        } else {
            DBG_LOG("M2M module fault.");
            M2M_Param.status = M2M_status_fault;
            M2M_KEY_OFF();
            M2M_PWR_OFF();
        }
    }
    if (M2M_Param.status == M2M_status_nocard) {
        DBG_LOG("M2M module no SIM card.");
        M2M_KEY_OFF();
        M2M_PWR_OFF();
    }
    return r;
}

/**
 * M2M�ػ��ص�
 */
static BOOL M2M_ModulePowerOff(void) {
    BOOL r = FALSE;
    M2M_KEY_ON();
    if (M2M_WAIT_ACK("POWER DOWN", 3000)) {
        r = TRUE;
    }
    M2M_KEY_OFF();
    M2M_PWR_OFF();
    return r;
}

/**
 * M2M�ر�������PDP����
 * @return ���عرս��
 */
static BOOL ConnectShut(void) {
    M2M_SEND_AT("QIDEACT=1");
    return M2M_WAIT_ACK("OK", 3000) ? TRUE : FALSE;
}

/**
 * M2M�ر�����
 * @return ���عرս��
 */
static BOOL ConnectClose(void) {
    M2M_SEND_AT("QICLOSE=0");
    return M2M_WAIT_ACK("CLOSE OK", 10000) ? TRUE : FALSE;
}

/**
 * M2M��������
 * @param addr ���ӵ�IP��ַ��������
 * @param port ���ӵĶ˿ں�
 * @return �������ӽ��
 */
static BOOL ConnectStart(char* addr, uint16_t port) {
    char* p = NULL;
    M2M_SEND_AT("QIACT?");
    p = M2M_WAIT_TOKEN("+QIACT:", 1000);
    if (p != NULL) {
        while (*p && *p++ != ',');
        if (*p == '1') {
            M2M_AT_PRINTF("QIOPEN=1,0,\"TCP\",\"%s\",%d,0,1", addr, port);
            if (M2M_WAIT_ACK("OK", 3000)) {
                return TRUE;
            }
        }
    }
    M2M_SEND_AT("QIACT=1");
    M2M_WAIT_ACK("OK", 3000);
    return FALSE;
}

/**
 * ��ȡRSII��ֵ
 * @return ����RSSI��ֵ
 */
static uint8_t GetRSSI(void) {
    uint8_t rssi = 0;
    char* p = NULL;
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
 * ��ȡRSII��ֵ
 * @return ����RSSI��ֵ
 */
static IP_Status_t GetIPStatus(void) {
    uint8_t ret = 5;
    char* p = NULL;
    M2M_SEND_AT("QISTATE=1,0");
    p = M2M_WAIT_TOKEN("+QISTATE:", 1000);
    if (p != NULL) {
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        ret = uatoi(p);
    }
    return (IP_Status_t)ret;
}

/**
 * ��ȡ����״̬
 * @return ��������ע��״̬
 */
static BOOL GetNetWorkStatus(void) {
    uint8_t net = 0;
    char* p = NULL;
    M2M_SEND_AT("CREG?");
    p = M2M_WAIT_TOKEN("+CREG:", 1000);
    if (p != NULL) {
        while (*p && *p++ != ',');
        net = uatoi(p);
    }
    return (net == 1 || net == 5) ? TRUE : FALSE;
}

/**
 * ���ֻ�SIM���绰����
 * @param num  ���뷵�ص�ָ��
 * @return ���ض������
 */
static BOOL ReadPhoneNum(char* num) {
    char* p = NULL;
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
 * TCPIP��������
 * @param data ����ָ��
 * @param len  ���ݳ���
 * @return ���ط��ͽ��
 */
static uint16_t TCPIP_Send(uint8_t* data, uint16_t len) {
    uint16_t sent = 0;
    if (data != NULL && len > 0) {
        M2M_AT_PRINTF("QISEND=0,%d", len);
        if (M2M_WAIT_TOKEN(">", 1000)) {
            M2M_SEND_DATA(data, len);
            M2M_SEND_DATA("\x1A", 1);
            if (M2M_WAIT_ACK("SEND OK", 3000)) {
                sent = len;
            }
        }
    }
    return sent;
}

/**
 * ����TTS
 *
 * @param text   ���������ı�
 */
static void  TTS_Play(char* text) {
    M2M_AT_PRINTF("QTTS=2,\"%s\"", text);
    M2M_WAIT_ACK("OK", 3000);
}

/**
 * M2M�ȴ�AT�����
 * @param token �ȴ���token
 * @param time  �ȴ����ʱ��
 * @return ���صȴ���token,��ʱ����NULL
 */
static char* WaitATRsp(char* token, uint16_t time) {
    uint16_t len = 0;
    uint32_t ts = 0;
    char* psearch = NULL;
    ts = HAL_GetTick();
    while (HAL_GetTick() - ts <= time) {
        len = UART_DataSize(M2M_UART_PORT);
        if (len > 0 && UART_GetDataIdleTicks(M2M_UART_PORT) >= 10) {
            /*����δ���������Ӱ������ָ��*/
            if ((UART_QueryByte(M2M_UART_PORT, len - 1) == '\n' && UART_QueryByte(M2M_UART_PORT, len - 2) == '\r')
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
                    psearch = (char*)SearchMemData(pRspBuf, (uint8_t*)token, len, strlen(token));
                    if (psearch != NULL || strstr((char*)pRspBuf, "ERROR")) {
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
 * M2M��������
 * @param argc ����������
 * @param argv �����б�
 */
static void M2M_Console(int argc, char* argv[]) {
    argv++;
    argc--;
    if (strcmp(argv[0], "power") == 0) {
        if (strcmp(argv[1], "on") == 0) {
            M2M_PWR_ON();
        } else if (strcmp(argv[1], "off") == 0) {
            M2M_PWR_OFF();
        }
        DBG_LOG("M2M power:%s", argv[1]);
    }  else if (strcmp(*argv, "status") == 0) {
        DBG_LOG("M2M status:%d, IP status:%d.", (int)M2M_Param.status, M2M_Param.IP_Status);
    }  else if (strcmp(*argv, "key") == 0) {
        if (strcmp(argv[1], "on") == 0) {
            M2M_KEY_ON();
        } else if (strcmp(argv[1], "off") == 0) {
            M2M_KEY_OFF();
        }
        DBG_LOG("M2M power key:%s", argv[1]);
    } else if (strcmp(argv[0], "phonenum") == 0) {
        char* num = NULL;
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
            M2M_SEND_DATA((uint8_t*)argv[1], strlen(argv[1]));
            DBG_LOG("M2M test send data OK.");
        } else if (strcmp(argv[0], "power") == 0) {
            if (strcmp(argv[1], "on") == 0) {
                M2M_PWR_ON();
            } else if (strcmp(argv[1], "off") == 0) {
                M2M_PWR_OFF();
            }
            DBG_LOG("M2M test power:%s", argv[1]);
        } else if (strcmp(argv[0], "tts") == 0) {
            TTS_Play(argv[1]);
            DBG_LOG("TTS_Play done.");
        } else if (strcmp(argv[0], "connectshut") == 0) {
            ConnectShut();
            DBG_LOG("ConnectShut done.");
        } else if (strcmp(argv[0], "errorcount") == 0) {
            M2M_Param.ErrorCount = uatoi(argv[1]);
            DBG_LOG("ErrorCount set done.");
        }
        osMutexRelease(M2M_MutexId);
    }
}

