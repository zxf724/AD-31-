/*****************************************************************************
 *
 * 文件描述: 协议头文件
 *
 * 修改记录:
 *
 *           1. 2016-7-12 chenyue Create
 *****************************************************************************/

#ifndef __PROC_H__
#define __PROC_H__
/*****************************************************************************
**                              宏定义
******************************************************************************/

#define PROC_TIME_RX            3

#define PROC_FLAG_RECVOK        10

#define PROC_TXBUF_SIZE         48 // 32 // 24
#define PROC_RXBUF_SIZE         24

#define PROC_OFFSET_HEAD        0
#define PROC_OFFSET_ID          1
#define PROC_OFFSET_CMD         5
#define PROC_OFFSET_LEN         6
#define PROC_OFFSET_DATASTART   7

#define PROC_BIT_NOACK          0
#define PROC_BIT_DEAL           0x01
#define PROC_BIT_ACK            0x02
#define PROC_BIT_DEALANDACK     0x03


/*****************************************************************************
**                              函数声明
******************************************************************************/


#define PROC_SendBuf(PBUF,LEN)          CMD_SendData(PBUF,LEN)
#define PROC_Crc(BUF,LEN,INIT)          CRC_16(INIT,BUF,LEN)

extern uint32 PROC_MY_SN;

extern void PROC_Init(void);
extern void PROC_Deal(uint8 *aucRevBuf, uint8 ucLen);
extern void PROC_TimeDeal(void);
extern void PROC_RxIrq(uint8 ucValue);


extern void PROC_SetCmd(uint8 cmd);
extern void PROC_AddData(uint8 ucId, uint8 ucDataLen, uint8 *pData);
extern void PROC_AddBuf(uint8 *pData, uint8 ucDataLen);
extern void PROC_Send(void);

#endif
/*****************************************************************************
**                            End of File
******************************************************************************/

