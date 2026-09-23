#ifndef __BUSINESS_H
#define __BUSINESS_H
#include <stdint.h>

typedef struct
{
    uint32_t seq;

    /*扩展坞报文字段*/
    uint16_t year;
    uint8_t mon;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t mem_usage;
    uint8_t cpu_usage;
    uint8_t disk_usage;

    /*环境感知报文字段*/
    float temp;
    uint8_t hum;
    uint16_t press;
    uint16_t alt;
    uint8_t o2;
    uint8_t co;
    uint8_t h2s;
    uint16_t ch4;

    /*硬件状态*/
    uint8_t backlight_en;
    uint8_t backlight_level; /*1‑5档*/
    uint8_t beep_en;
}business_state_t;

extern business_state_t g_dev_state;
int business_init(void);
/*用系统本地时间刷新报文时间字段，建议每秒调用一次*/
void business_update_time(void);
/*PTU校时：更新业务状态并设置系统时钟(需root)，成功返回0*/
int business_sync_time(uint16_t year, uint8_t mon, uint8_t day,
                       uint8_t hour, uint8_t min, uint8_t sec);
#endif
