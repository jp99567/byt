/*
 * Log.cpp
 *
 *  Created on: May 21, 2017
 *      Author: j
 */

#include "Log.h"

namespace Util {

Log::~Log() {
	logger->debug("logger finished");
}
Log::Log() {
	//logger = spdlog::stdout_color_mt("console");
	logger = spdlog::stderr_logger_mt("bytdlog");
	logger->set_level(spdlog::level::debug);
}

void Log::syserr(const char* msg)
{
	auto tmperno = errno;
	if(msg)
		logger->error("{} syserr: {}", msg, strerror(tmperno));
	else
		logger->error("syserr: {}", strerror(tmperno));
}

void Log::sysdie()
{
	auto tmperno = errno;
	logger->error("syserr: {}", strerror(tmperno));
	throw std::runtime_error(strerror(tmperno));
}

Log& Log::instance()
{
	static Log _;
	return _;
}

} /* namespace Util */
