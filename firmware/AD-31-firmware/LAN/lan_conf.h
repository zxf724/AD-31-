/*
 * @Description: lan配置文件
 * @Author: alexy
 * @LastEditors: Please set LastEditors
 * @Date: 2019-04-09 21:20:10
 * @LastEditTime: 2019-04-09 21:41:15
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LAN_CONF_H
#define __LAN_CONF_H

/* Includes ------------------------------------------------------------------*/



/* Exported define -----------------------------------------------------------*/
#define NET_LAN_EN   1
#define VERSION_LAN         "W5500"

#define W5500_CS_Pin GPIO_PIN_4
#define W5500_CS_GPIO_Port GPIOA
#define W5500_INT_Pin GPIO_PIN_4
#define W5500_INT_GPIO_Port GPIOC
#define W5500_RST_Pin GPIO_PIN_5
#define W5500_RST_GPIO_Port GPIOC
/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/



#endif
