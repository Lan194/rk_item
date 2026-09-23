#ifndef __HW_DRIVER_H
#define __HW_DRIVER_H
int hw_driver_init(void);
int hw_backlight_switch(int en);
int hw_backlight_set_level(int level);
int hw_beep_set(int en);
#endif
