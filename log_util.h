#ifndef __LOG_UTIL_H
#define __LOG_UTIL_H
#include <stdio.h>
#define LOG_I(fmt,...) printf("[I]"fmt,##__VA_ARGS__)
#define LOG_E(fmt,...) printf("[E]"fmt,##__VA_ARGS__)
#endif
