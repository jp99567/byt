#define THREAD_UTIL_CPP_

#include <csignal>
#include <pthread.h>
#include "Log.h"

namespace thread_util
{

bool shutdown_req(false);

void sigblock(bool block, bool block_alarm)
{
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	int err = pthread_sigmask(block ? SIG_BLOCK : SIG_UNBLOCK, &set, nullptr);
	if(err) LogSYSDIE();
	sigemptyset(&set);
	sigaddset(&set, SIGALRM);
	err = pthread_sigmask(block_alarm ? SIG_BLOCK : SIG_UNBLOCK, &set, nullptr);
	if(err) LogSYSDIE();
}

void set_thread_name(const char *name)
{
	pthread_setname_np(pthread_self(), name);
}

static void sig_hdl(int sig)
{
	shutdown_req = true;
}

void set_sig_handler()
{
	std::signal(SIGINT, sig_hdl);
	std::signal(SIGTERM, sig_hdl);
	std::signal(SIGALRM, [](int sig){
		LogERR("alarm");
	});
}

}
