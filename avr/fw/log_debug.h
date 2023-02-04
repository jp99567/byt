#pragma once

//#define LOG_DEBUG 1

void init_log_debug();
#ifdef LOG_DEBUG

#include <stdio.h>

#define DBG(fmt, ...) do { \
	printf(fmt"\n", ##__VA_ARGS__); \
} while(0)

#else
#define DBG(fmt, ...) do{}while(0)
#endif
