/*****************************************************************************
 *
 * 文件描述: 参数处理头文件
 *
 * 修改记录:
 *
 *           1. 2016-7-25 chenyue Create
 *****************************************************************************/

#ifndef __PARAM_H__
#define __PARAM_H__
/*****************************************************************************
**                              宏定义
******************************************************************************/

#include "user_comm.h"


/* 基础参数类型 -- 根据芯片ROM与需求选择适合的数据宽度 */
#define PARAM_T     uint16
#define PARAM_T2    uint32
// 存储在FLASH中 -- 每个对应的内存参数初始值视为默认值
typedef struct stgPARAM_SYSCONFIG_S
{
    uint8 aucRes0[2];           // 此处地址无效
    uint16 usParamIndexMax;     // 最大有效索引 0 - 0xffff 无效
    /* 添加参数>>> */
    lstring szDomain;           // 域名或IP地址
    uint16 usPort;              // 端口
    uint8 aucRes1[2];

    uint32 aulPmetSn[16]; // [PMET_TERMINAL_CNT];
    uint8 ucPmetElecHeapMonth;    // 电量当前累积的月份
    uint8 ucPmetElecHeapYear; //  电量累积的年份
    lstring szClientId;         // 终端ID

    /* <<<< 添加参数 */
}PARAM_SYSCONFIG_S;


#define     PARAM_SAVE_ADDR()                           (SECTOR_ADDR(SECTOR_PARAM_START))
#define     PARAM_SYSCONFIG_ADDR                        (PARAM_SAVE_ADDR())

#define     GET_STRUCT_MEMBER_SIZE(type, MEMBER)        sizeof(((type *)0)->MEMBER)          // 获取结构体成员所占空间
#define     GET_STRUCT_MEMBER_OFFSET(type, MEMBER)      ((uint32)&((type *)0)->MEMBER)       // 获取结构体成员偏移地址

#define     GET_SYSCFG_MEMBER_ADDR(MEMBER)              (PARAM_SYSCONFIG_ADDR + (GET_STRUCT_MEMBER_OFFSET(PARAM_SYSCONFIG_S, MEMBER)))
#define     GET_SYSCFG_MEMBER_SIZE(MEMBER)              (GET_STRUCT_MEMBER_SIZE(PARAM_SYSCONFIG_S, MEMBER))          // 获取结构体成员所占空间


#define PARAM_Erase(ADDR,NUM)           SFlash_EraseSectors(ADDR,NUM)
#define PARAM_Read(ADDR,PBUF,LEN)       SFlash_Read(ADDR,PBUF,LEN)
#define PARAM_Write(ADDR,PDATA,LEN)     SFlash_Write(ADDR,PDATA,LEN)


typedef union tagPARAM_TYPE_U
{
    uint8 ucData;
    uint16 usData;
    uint32 ulData;
    lstring szLStr;
    mstring szMStr;
    sstring szSStr;
    char acData[TYPE_LSTRING_LEN];
    uint8 aucData[TYPE_LSTRING_LEN];
}PARAM_TYPE_U;

typedef struct tagPARAM_INFO_S
{
    PARAM_T ucId;
    uint8 ucLen;
    PARAM_T2 ucSaveAddr;
    void *pValue;
}PARAM_INFO_S;

/* 参数ID */
typedef enum tagPARAM_ID_E
{
    PARAM_ID_INDEXMAX = 0x71, // 此值不可更改
    PARAM_ID_DOMAIN = 0x72,
    PARAM_ID_PORT = 0x73,

    PARAM_ID_ARRSN = 0x74,
    PARAM_ID_ELECMONTH = 0x75,
    PARAM_ID_ELECYEAR = 0x76,
    PARAM_ID_CLIENTID,

    PARAM_ID_MAX,
}PARAM_RAMID_E;


/*****************************************************************************
**                              函数声明
******************************************************************************/

extern const PARAM_INFO_S g_astParamInfo[];

extern void PARAM_Init(void);
extern void PARAM_Reset(void);

extern PARAM_T PARAM_SearchIndex(PARAM_T ucId);
extern void PARAM_SaveAll(void);
extern void PARAM_UpdateRamData(PARAM_T ucIndex);
extern void PARAM_SaveFromMemAddr(void *pMemAddr);

extern void PARAM_BaseTypeToBuf(uint8 *pBuf, void *pValue, uint32 ulLen);
extern void PARAM_BaseTypeFromBuf(uint8 *pBuf, void *pValue, uint32 ulLen);

#endif
/*****************************************************************************
**                            End of File
******************************************************************************/

