/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file RTC_ext.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.4.1
 * @brief 外置RTC驱动函数头文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _USER_RTC_H
#define _USER_RTC_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"

#include "time.h"

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void ExternRTC_Init(void);
BOOL RTC_ReadTime(struct tm* time);
BOOL RTC_ReadTimeStr(char *buf);
BOOL RTC_SetTime(struct tm* time);

void RTC_TickToStr(uint32_t ts, char *buf);
void RTC_TimeToStr(struct tm* time, char *buf);

void RTC_TickToTime(uint32_t tick, struct tm* time);
uint32_t RTC_TimeToTick(struct tm* tim);

uint32_t RTC_ReadTick(void);

#endif
