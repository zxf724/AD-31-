/*****************************************************************************
 *
 * 文件描述: SFlash记录管理
 *
 * 修改记录:
 *
 *           1. 2015-1-20 chenyue Create
 *****************************************************************************/

#ifndef __RECMANAGE_H__
#define __RECMANAGE_H__
/*****************************************************************************
**                              宏定义
******************************************************************************/

#include "user_comm.h"


#define FLASH_Erase(ADDR)                   SFlash_EraseSectors_NotCheck(ADDR,1)
#define FLASH_Read(ADDR,PDATA,LEN)          SFlash_Read(ADDR, PDATA, LEN)
#define FLASH_OnlyWrite                     SFlash_Write


#define OSSchedLock()                       vTaskSuspendAll()
#define OSSchedUnlock()                     xTaskResumeAll()

#define RECM_READRECINDEX(SADDR,TADDR)      *(TADDR)=*(SADDR)
#define RECM_WRITERECINDEX(TADDR, SADDR)    *(TADDR)=*(SADDR)
/* 擦除单位4k */
#define RECM_BLOCK_SIZE                 SFLASH_SECTOR_SIZE

#define RECM_DWORD_SIZE                 4

/* SFLASH模块管理的FLASH存储空间起始，由CONFIG.H进行定义 */
/* FLASH_BLOCK_SFLASHSTART */

/* 根据单条记录的长度，获取单块所能存储的条数 */
#define RECM_GETSAVEUNITLEN(UNITLEN)        (UNITLEN)
#define RECM_GETBLOCKUNITNUM(UNITLEN)       RECM_GETSAVEUNITLEN(UNITLEN),\
                                            (RECM_BLOCK_SIZE/RECM_GETSAVEUNITLEN(UNITLEN))


/* 行驶速度记录 每条记录16 一块4096 一块能存256条记录 要求最少存储48小时数据 2880条记录 */
/* 记录长度(2) 时间(6) 事件(1) 通道(1) 相关参数1(2) 相关参数2(4) */
#define RECM_UNITLEN_EVEN               16
#define RECM_BLOCKSTART_EVEN            SECTOR_RECM_START
#define RECM_BLOCKTATOL_EVEN            (1 + 1)

/* 分路电量累积 */
#define RECM_UNITLEN_ELEC               (4 + (16 * PMET_CHANNEL_CNT))
#define RECM_BLOCKSTART_ELEC            (RECM_BLOCKSTART_EVEN + RECM_BLOCKTATOL_EVEN)
#define RECM_BLOCKTATOL_ELEC            (32)

#ifdef SECTOR_RECM_START
/* 以上记录共占用XX块 */
#define RECM_FLASH_ENDADDR              (RECM_BLOCKSTART_ELEC + RECM_BLOCKTATOL_ELEC)
#if((RECM_FLASH_ENDADDR) > (SECTOR_RECM_START + SECTOR_RECM_SIZE))
/* 空间不足 */
#error "RECMANAGE FLASH SPACE ERR"
#endif
#endif

typedef struct tagRECM_INFO_S
{
    uint8  ucRecType;
    uint8  aucRes[1];
    uint16 ulUnitLen;           /* 单个记录长度 */
    uint32 ulBlockUnitNum;      /* 单块存储记录条数 */
    uint32 *pulWIndexId;          /* 写索引FM参数ID */
    uint32 *pulRIndexId;          /* 读索引FM参数ID */
    uint32 ulStartBlockNum;     /* 起始块号 */
    uint32 ulTotalBlockNum;     /* 总块数 */
}RECM_INFO_S;

/*
01H 事件记录
*/

typedef enum tagRECM_RECTYPE_E
{
    RECM_RECTYPE_EVEN = 0x01,
    RECM_RECTYPE_ELEC,
}RECM_RECTYPE_E;

#define RECM_INDEX_MAX        0xffffffff


/*****************************************************************************
**                              函数声明
******************************************************************************/


//#define RECM_AddRec(RECTYPE,PDATA)   RECM_AddRecEx(RECTYPE,PDATA,0xffff)
extern uint32 RECM_AddRec(uint8 ucRecType, uint8 *pData);
extern uint8 RECM_ModData(uint8 ucRecType, uint32 ulIndex, uint32 ulOffset, uint8 *pBuf, uint32 ulMaxLen);
extern uint32 RECM_AddRecEx(uint8 ucRecType, uint8 *pData, uint16 usLen, uint8 *pErr);

extern uint32 RECM_GetRecTotal(uint8 ucRecType);
extern uint32 RECM_GetOldestIndex(uint8 ucRecType);
extern uint32 RECM_GetLastIndex(uint8 ucRecType);
extern uint32 RECM_ReadRec(uint8 ucRecType, uint32 ulIndex, uint8 *pBuf, uint32 ulMaxLen);
extern uint32 RECM_ReadOnlyTime(uint16 usTimeOffset, uint8 ucRecType, uint32 ulIndex, uint8 *pBuf, uint32 ulMaxLen);

extern RECM_INFO_S *RECM_GetInfoAddr(uint8 ucRecType);

extern void RECM_Init(void);

#endif
/*****************************************************************************
**                            End of File
******************************************************************************/


