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
#ifndef WITHOUT_SPDLOG
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
#else
    static Log& instance(){static Log _; return _;}
    void syserr(const char* msg = nullptr){}
    void die(const char* msg = nullptr){}
    void sysdie(const char* msg = nullptr){}
#endif
};

} /* namespace Util */

#ifndef WITHOUT_SPDLOG
#define LogINFO(...) Util::Log::instance().get()->info(__VA_ARGS__)
#define LogERR(...) Util::Log::instance().get()->error(__VA_ARGS__)
#define LogDBG(...) Util::Log::instance().get()->debug(__VA_ARGS__)
#define LogSYSDIE(msg) Util::Log::instance().sysdie(msg)
#define LogSYSERR(msg) Util::Log::instance().syserr(msg)
#define LogDIE(...) { Util::Log::instance().get()->error(__VA_ARGS__); Util::Log::instance().die(); }
#else
#define LogINFO(...)
#define LogERR(...)
#define LogDBG(...)
#define LogSYSDIE(msg)
#define LogSYSERR(msg)
#define LogDIE(...)
#endif
