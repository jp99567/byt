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

private:
	Log();

	std::shared_ptr<spdlog::logger> logger;
};

} /* namespace Util */

#define LOG_INFO(...) Util::Log::instance().get()->info(__VA_ARGS__)
#define LOG_ERR(...) Util::Log::instance().get()->error(__VA_ARGS__)
#define LOG_DBG(...) Util::Log::instance().get()->debug(__VA_ARGS__)

