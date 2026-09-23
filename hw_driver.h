/**
 * @file hw_driver.h
 * @brief RK3506 硬件驱动接口
 *
 * 覆盖两类外设：
 *   1. 蜂鸣器 —— 通过 sysfs GPIO 24 控制，高电平响低电平停
 *   2. 背光   —— 通过 /sys/class/backlight/<dev>/ 控制
 *                hw_backlight_switch() 开关，hw_backlight_set_level() 调档(1~5)
 *
 * 所有接口内部自行探测设备路径，调用者无需关心具体硬件节点名。
 */
#ifndef __HW_DRIVER_H
#define __HW_DRIVER_H

/** @brief 初始化所有硬件（导出 GPIO、关闭蜂鸣器、探测背光设备） */
int hw_driver_init(void);

/**
 * @brief 背光开关
 * @param en 1=点亮, 0=关闭（通过 bl_power=0/4）
 */
int hw_backlight_switch(int en);

/**
 * @brief 背光档位调节
 * @param level 1~5 档，线性映射到 max_brightness 的 20%~100%
 */
int hw_backlight_set_level(int level);

/**
 * @brief 蜂鸣器开关
 * @param en 1=响, 0=停
 */
int hw_beep_set(int en);

#endif
