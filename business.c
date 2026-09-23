#define _POSIX_C_SOURCE 200809L
#include "business.h"
#include "log_util.h"
#include <time.h>
#include <errno.h>
#include <string.h>

business_state_t g_dev_state = {
    .seq = 0,
    .year =2026,.mon=9,.day=23,.hour=12,.min=0,.sec=0,
    .mem_usage=30,.cpu_usage=20,.disk_usage=25,
    .temp=25.5f,.hum=45,.press=980,.alt=120,.o2=21,.co=0,.h2s=0,.ch4=0,
    .backlight_en=1,.backlight_level=3,.beep_en=0
};

void business_update_time(void)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if(t == NULL) return;
    g_dev_state.year = t->tm_year+1900;
    g_dev_state.mon  = t->tm_mon+1;
    g_dev_state.day  = t->tm_mday;
    g_dev_state.hour = t->tm_hour;
    g_dev_state.min  = t->tm_min;
    g_dev_state.sec  = t->tm_sec;
}

int business_sync_time(uint16_t year, uint8_t mon, uint8_t day,
                       uint8_t hour, uint8_t min, uint8_t sec)
{
    /*合法性校验，拒绝非法时间*/
    if(year < 2000 || year > 2099 ||
       mon < 1 || mon > 12 || day < 1 || day > 31 ||
       hour > 23 || min > 59 || sec > 59)
    {
        LOG_E("time-sync invalid: %04u-%02u-%02u %02u:%02u:%02u\n",
              year, mon, day, hour, min, sec);
        return -1;
    }

    /*先更新业务状态，即使系统时钟设置失败(非root)，上报时间仍以PTU校时为准*/
    g_dev_state.year = year;
    g_dev_state.mon  = mon;
    g_dev_state.day  = day;
    g_dev_state.hour = hour;
    g_dev_state.min  = min;
    g_dev_state.sec  = sec;

    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = year - 1900;
    t.tm_mon  = mon - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = min;
    t.tm_sec  = sec;
    t.tm_isdst = -1; /*由系统判断夏令时，国内环境无影响*/

    time_t ts = mktime(&t);
    if(ts == (time_t)-1)
    {
        LOG_E("time-sync mktime fail\n");
        return -1;
    }

    struct timespec tv;
    tv.tv_sec  = ts;
    tv.tv_nsec = 0;
    if(clock_settime(CLOCK_REALTIME, &tv) != 0)
    {
        LOG_E("clock_settime fail: %s (need root?)\n", strerror(errno));
        return -1;
    }

    LOG_I("system time synced: %04u-%02u-%02u %02u:%02u:%02u\n",
          year, mon, day, hour, min, sec);
    return 0;
}

int business_init(void)
{
    LOG_I("business init ok\n");
    business_update_time();
    return 0;
}
