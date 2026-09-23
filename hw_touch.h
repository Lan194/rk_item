/**
 * @file hw_touch.h
 * @brief 触摸屏输入接口（ADS7846 电阻屏）
 *
 * 设计要点：
 *   - 自动探测 /dev/input/event* 中支持 EV_ABS 的设备
 *   - 以非阻塞方式读取 input_event，主循环轮询无延迟
 *   - 内核驱动上报的原始值（0~4095，12-bit ADC）会被归一化
 *     到协议要求的 0~2048 范围，并处理 X 轴方向反转
 *   - 触摸抬起瞬间坐标归零，避免上位机显示"卡住"
 */
#ifndef __HW_TOUCH_H
#define __HW_TOUCH_H

#include <stdint.h>

/**
 * @brief 初始化：自动探测触摸设备并打开（失败不致命，后续 poll 返回 -2）
 * @return 0=成功, -1=未找到设备
 */
int  hw_touch_init(void);

/**
 * @brief 非阻塞轮询触摸坐标
 * @param x       [out] 映射后的 X 坐标（0~2048，左=0 右=2048）
 * @param y       [out] 映射后的 Y 坐标（0~2048，上=0 下=2048）
 * @param pressed [out] 1=按下, 0=抬起
 * @return 0=有新事件, -1=无新数据, -2=设备错误
 */
int  hw_touch_poll(uint16_t *x, uint16_t *y, uint8_t *pressed);

/** @brief 关闭触摸设备 */
void hw_touch_close(void);

#endif
