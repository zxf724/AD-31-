/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file rtc_ext.c
 * @version V1.0
 * @date 2016.4.1
 * @brief 外置RTC驱动函数文件.
 *
 * *********************************************************************
 * @note
 * 2016.12.15 增加RTC_TickToStr与RTC_TimeToStr函数
 * 2016.12.26 增加tick转换查询命令
 *
 * *********************************************************************
 * @author 宋阳
 */



/* Includes ------------------------------------------------------------------*/
#include "user_comm.h"
#include "time.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/



/* Private function prototypes -----------------------------------------------*/

static time_t rtc_tick = 0;
/* Exported functions --------------------------------------------------------*/
/**
 * 初始化RTC 驱动.
 */
void rtc_init(void)
{
    rtc_tick = 0;
}
/*
可以在项目设置中设置时间使用的变量类型计数范围
_DLIB_TIME_USES_LONG
_DLIB_TIME_USES_64
*/
clock_t clk_count = 0;//系统上电后运行时间计数，如果是32位，1ms周期下最大计时49.7天
time_t time_dat;//设置localtime相对于公元1970年1月1日0时0分0秒算起至今的UTC时间所经过的秒数
 
/* Private functions ---------------------------------------------------------*/
clock_t clock (void) {
    return HAL_GetTick();
}
 
#if _DLIB_TIME_USES_64
time_t __time64 (time_t *p) {
    return time_dat;
}
#else
time_t __time32 (time_t *p) {
    return time_dat;
}
#endif
 
const char * __getzone(void)
{
    return ": GMT+8:GMT+9:+0800";
}

/**
 * @brief
 * @note tick++
 * @retval None
 */
__IO void tick_up(void) {
    rtc_tick++;
}


/**
 * RTC读出时间的字符串
 * @param buf  读出的位置
 * @return 返回读出结果
 */
BOOL RTC_ReadTimeStr(char *buf)
{
    struct tm* temp;
    time_t now;
    now = time((time_t *)NULL);
    temp = localtime(&now);
    if (temp) {
        sprintf(buf, "%04d-%02d-%02d-%02d:%02d:%02d", 1990 + temp->tm_year,
                temp->tm_mon, temp->tm_mday, temp->tm_hour, temp->tm_min, temp->tm_sec);
        return TRUE;
    }
    return FALSE;
}

/**
  * @brief  写RTC.
  * @param  time: 数据读入的指针.
  * @retval 写入成功返回TRUE.
  */
void rtc_tick_set(time_t temp){
    rtc_tick = temp;
}


/**
 * RTC时间戳转为字符串输出
 * @param ts   输入的时间戳
 * @param buf  输出的字符串
 */
void RTC_TickToStr(uint32_t ts, char *buf)
{
    struct tm* time;
    time = localtime(&ts);
    sprintf(buf, "%04d-%02d-%02d-%02d:%02d:%02d", 1990 + time->tm_year,
                time->tm_mon, time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);
}






