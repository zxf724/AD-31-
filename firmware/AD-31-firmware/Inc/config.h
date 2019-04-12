/*****************************************************************************
 *
 * 文件描述: 配置
 *
 * 修改记录:
 *
 *           1. 2017-1-3 chenyue Create
 *****************************************************************************/

#ifndef __CONFIG_H__
#define __CONFIG_H__
/*****************************************************************************
**                              宏定义
******************************************************************************/


/* 数据类型定义 */
typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int    uint32;

#define TYPE_UINT8_LEN      1
#define TYPE_UINT16_LEN     2
#define TYPE_UINT32_LEN     4

#define TYPE_LSTRING_LEN    32
#define TYPE_MSTRING_LEN    16
#define TYPE_SSTRING_LEN    8
typedef char lstring[TYPE_LSTRING_LEN];
typedef char mstring[TYPE_MSTRING_LEN];
typedef char sstring[TYPE_SSTRING_LEN];




/*****************************************************************************
**                              函数声明
******************************************************************************/

#if CJSON_EN && CJSON_EN > 0
#include "cJSON.h"
#endif

#include "PowerMeter.h"
#include "Proc.h"
#include "Param.h"
#include "RecManage.h"



#endif
/*****************************************************************************
**                            End of File
******************************************************************************/
