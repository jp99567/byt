/*
 * thread_util.h
 *
 *  Created on: Jan 11, 2015
 *      Author: j
 */

#ifndef THREAD_UTIL_H_
#define THREAD_UTIL_H_

namespace thread_util
{

#ifndef THREAD_UTIL_CC_
extern bool shutdown_req;
#endif

void sigblock(bool block, bool block_alarm);
void set_thread_name(const char *name);
void set_sig_handler();

}

#endif /* THREAD_UTIL_H_ */
