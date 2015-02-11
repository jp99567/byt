/*
 * log_trace_debug.h
 *
 *  Created on: Jan 9, 2015
 *      Author: j
 */

#include <stdio.h>

#ifndef LOG_TRACE_DEBUG_H_
#define LOG_TRACE_DEBUG_H_

#define SYSERR ltd::logdebugtrace(__FILE__,__LINE__,nullptr,false,true,true)
#define SYSDIE ltd::logdebugtrace(__FILE__,__LINE__,nullptr,true,true,true)

#define ERRORS(...){\
	char _str[256];\
	snprintf(_str,255,__VA_ARGS__);\
	ltd::logdebugtrace(__FILE__,__LINE__,_str,false,false,true);}

#define DIES(...){\
	char _str[256];\
	snprintf(_str,255,__VA_ARGS__);\
	ltd::logdebugtrace(__FILE__,__LINE__,_str,true,false,true);}

#define INFO(...){\
	char _str[256];\
	snprintf(_str,255,__VA_ARGS__);\
	ltd::logdebugtrace(__FILE__,__LINE__,_str,false,false,false);}

namespace ltd
{

void logdebugtrace(const char* file, const int line, const char* msg, bool die, bool syserr, bool outstderr);

}

#endif /* LOG_TRACE_DEBUG_H_ */
