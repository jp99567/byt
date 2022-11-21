/*
 * Log.h
 *
 *  Created on: May 21, 2017
 *      Author: j
 */

#pragma once
#include <spdlog/spdlog.h>

namespace Util {

class Log {
public:
	~Log();

	static Log& instance();

	std::shared_ptr<spdlog::logger> get(){
		return logger;
	}

	void syserr(const char* msg = nullptr);
	void die(const char* msg = nullptr);
    void sysdie(const char* msg = nullptr);

private:
	Log();

	std::shared_ptr<spdlog::logger> logger;
};

class LogExit
{
	const std::string msg;
public:
	LogExit(std::string msg) : msg(std::move(msg)){}
	~LogExit()
	{
		Log::instance().get()->debug("exit:{}", msg);
	}
};

} /* namespace Util */

#define LogINFO(...) Util::Log::instance().get()->info(__VA_ARGS__)
#define LogERR(...) Util::Log::instance().get()->error(__VA_ARGS__)
#define LogDBG(...) Util::Log::instance().get()->debug(__VA_ARGS__)
#define LogSYSDIE(msg) Util::Log::instance().sysdie(msg)
#define LogSYSERR(msg) Util::Log::instance().syserr(msg)
#define LogDIE(...) { Util::Log::instance().get()->error(__VA_ARGS__); Util::Log::instance().die(); }

