/**
 * @file business.h
 * @brief 业务层状态与接口
 *
 * g_dev_state 是全局唯一的业务状态结构体，
 * 同时作为两类上报报文的数据源：
 *   - pack_dock_status_frame()    取其中 year/mon/day/.../mem_usage/cpu_usage/disk_usage
 *   - pack_sensor_status_frame()  取其中 temp/hum/press/.../touch_x/touch_y/compass
 *
 * 状态更新来源：
 *   - business_update_time()     从系统时钟刷新时间字段
 *   - business_update_sysinfo()  从 /proc 刷新 CPU/内存/磁盘
 *   - business_sync_time()       PTU 校时指令下发
 *   - hw_touch_poll()            触摸设备驱动直接写入 touch_x/y/pressed
 *   - protocol_parse_ptu_cmd()   PTU 下行指令修改 backlight_en/level/beep_en
 */
#ifndef __BUSINESS_H
#define __BUSINESS_H
#include <stdint.h>

typedef struct
{
    uint32_t seq;               /* 报文序列号，每帧自增，用于上位机判断丢包 */

    /* ---- 扩展坞状态报文字段（dock 帧，out[8..18]） ---- */
    uint16_t year;
    uint8_t mon, day, hour, min, sec;
    uint8_t mem_usage;          /* 内存使用率 % */
    uint8_t cpu_usage;          /* CPU 使用率 % */
    uint8_t disk_usage;         /* 磁盘使用率 % */

    /* ---- 环境感知状态报文字段（sensor 帧，out[8..28]） ---- */
    float    temp;              /* 温度 °C，协议乘 10 发 uint16 */
    uint8_t  hum;               /* 湿度 % */
    uint16_t press;             /* 大气压 hPa */
    uint16_t alt;               /* 海拔 m */
    uint8_t  o2;                /* 氧气浓度 % */
    uint8_t  co;                /* 一氧化碳 ppm */
    uint8_t  h2s;               /* 硫化氢 ppm */
    uint16_t ch4;               /* 甲烷 LEL */

    /* ---- 触摸与罗盘（sensor 帧 out[24..29]） ---- */
    uint16_t touch_x;           /* 触摸 X 坐标 0~2048 */
    uint16_t touch_y;           /* 触摸 Y 坐标 0~2048 */
    uint8_t  touch_pressed;     /* 1=按下, 0=抬起 */
    uint16_t compass;           /* 罗盘航向角 °，当前硬置 0 */

    /* ---- 硬件状态（dock 帧隐含字段） ---- */
    uint8_t backlight_en;       /* 背光开关 */
    uint8_t backlight_level;    /* 背光档位 1~5 */
    uint8_t beep_en;            /* 蜂鸣器开关 */
} business_state_t;

extern business_state_t g_dev_state;

/** @brief 业务层初始化：调用 business_update_time() 同步一次系统时钟 */
int business_init(void);

/** @brief 从系统本地时间刷新 g_dev_state 的时间字段 */
void business_update_time(void);

/** @brief 从 /proc 读取 CPU/内存/磁盘利用率，建议每 500ms 调一次 */
void business_update_sysinfo(void);

/**
 * @brief PTU 下发校时指令：先更新 g_dev_state，再设置系统时钟
 * @return 0=全部成功, -1=参数非法或 mktime/clock_settime 失败
 * @note clock_settime 需要 root 权限；即使设置失败，g_dev_state 仍已更新，
 *       上位机下次收到的时间将以 PTU 下发值为准
 */
int business_sync_time(uint16_t year, uint8_t mon, uint8_t day,
                       uint8_t hour, uint8_t min, uint8_t sec);

#endif
