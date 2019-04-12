/*****************************************************************************
 *
 * 文件描述: 分机管理
 *
 * 修改记录:
 *
 *           1. 2016-9-13 chenyue Create
 *****************************************************************************/

#ifndef __POWERMETER_H__
#define __POWERMETER_H__
/*****************************************************************************
**                              宏定��?
******************************************************************************/


#define PMET_TASK_STK_SIZE          256
#define PMET_TASK_PRIO              osPriorityNormal

#define PMET_UART_BDR               115200
#define PMET_UART_REFRESH_TICK      1000

/*定义命令类型*/
#define PMET_CMD_CLEAR          0x00
#define PMET_CMD_ONLINE         0x01
#define PMET_CMD_INFO           0x02
#define PMET_CMD_RTSTATUS       0x03

#define PMET_CMD_READ_PARAM     0x04
#define PMET_CMD_WRITE_PARAM    0x05
#define PMET_CMD_SET_TEMPE      0X06 //温度
#define PMET_CMD_READ_ATT_PARA		0X08
#define PMET_CMD_SET_ATT_PARA		0X09
#define PMET_CMD_CLEAR_ATT_PARA		0X0A
#define PMET_CMD_SET_LIMITVOLAT_H	0X10
#define PMET_CMD_SET_LIMITVOLAT_L	0X11

#define PMET_CMD_SET_SWITCH     0x12
#define PMET_CMD_SET_CURRENT    0X13
#define PMET_CMD_SET_POWER      0X14
#define PMET_CMD_SET_PX         0X15
#define PMET_CMD_LEAKAGE_TEST	0X16
#define PMET_CMD_AUTO_ALLOW	    0X17


/* 字段偏移 */
#define PMET_OFFSET_HEAD        0
#define PMET_OFFSET_ADD         1
#define PMET_OFFSET_ID          2
#define PMET_OFFSET_CMD         6
#define PMET_OFFSET_LEN         7
#define PMET_OFFSET_DATASTART   8


/*参数ID定义*/
#define PARAM_ID_SWITCH         0x24

#define PMET_SN_NULL            0
#define PMET_TERMINAL_CNT       9 // 16 -- // 实际��?0路为��?
#define PMET_CHANNEL_CNT        32 // 32
#define PMET_LINE_TIMEDELAY     10
typedef struct {
    uint8_t     online;
    uint8_t     version;
    uint8_t     type;  
    int8_t      temperature;         // 温度 
    uint8_t     limittemp;
    uint8_t     switchstatus;       // 开
    uint8_t     thunderstatus; 
    uint8_t     autoallow;               //是否允许自动动作
    uint8_t     leakage;
    uint8_t     limitleakage;
    uint16_t    error;    
    uint16_t    powerfactor;    //功率因子
    uint32_t    sn;
    uint32_t    voltage[4];     // 电压 A B C (V)
    uint32_t    limitvoltageh;
    uint32_t    limitvoltagel;
    uint32_t    current[4];     // 电流 A B C D N (mA)
    uint32_t    limitcurrent;
    uint16_t    powerp;         //有功功率
    uint16_t    powerq;         //无功功率
    uint16_t    powers;         //
    uint32_t    limitpower;
    uint32_t    energyp;
    uint32_t    energyq;
    uint32_t    energys;

    //uint32 ulElecMonth;
    //uint32 ulElecLast;
    //uint32 ulElecYear;
}PMET_CHANNELINFO_S; //PMET_MAINCHANNELINFO_S;



#define TERMINAL_BUFSIZE_MAX  (sizeof(uint32) * PMET_TERMINAL_CNT)
#define TERMINAL_ADDR_START   SECTOR_ADDR(SECTOR_TERSN_START)


/*****************************************************************************
**                              函数声明
******************************************************************************/


//extern uint32 g_aulPmetElecInfo[];

extern uint8 g_ucPmetElecHeapDay;    // 电量当前累积的月��?
extern uint8 g_ucPmetElecHeapMonth;    // 电量当前的累积年��?



extern void  PMET_Init(void);
extern uint8 PMET_WaitAck(uint32 sn, uint8 cmd, uint8_t address);
extern uint8 PMET_GetChannelLineStatus(uint8 ucChannel);


extern PMET_CHANNELINFO_S *PMET_GetChnInfo(uint8 ucChannel);

extern uint32 *PMET_GetElecInfoAddr(uint8 ucChn);

extern void PMET_SetChannelSwitch(uint8 ucChannel,uint32_t sn, uint8 ucSwitch, uint8_t cmd);
extern void PMET_SetChannelParam(uint8 aucBuf[8]);
extern void PMET_SetChnParam(uint8 ucChannel,uint32_t sn, uint32 ulRatedCurrent,uint8 cmd);

extern void PMET_ElecReset(void);
extern void PMET_MakeElecReset(void);
#endif
/*****************************************************************************
**                            End of File
******************************************************************************/
