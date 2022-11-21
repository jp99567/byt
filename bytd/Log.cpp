/*
 * Log.cpp
 *
 *  Created on: May 21, 2017
 *      Author: j
 */

#include "Log.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace Util {

Log::~Log() {
	logger->debug("logger finished");
}
Log::Log() {
	logger = spdlog::stderr_color_mt("bytd");
	logger->set_level(spdlog::level::debug);
	auto owLogger = spdlog::stderr_color_mt("ow");
	owLogger->set_level(spdlog::level::info);
}

void Log::syserr(const char* msg)
{
	auto tmperno = errno;
	if(msg)
		logger->error("{} syserr: {}", msg, strerror(tmperno));
	else
		logger->error("syserr: {}", strerror(tmperno));
}

void Log::die(const char* msg)
{
	logger->flush();
	if(msg)
		throw std::runtime_error(msg);
	else
		throw std::runtime_error("die");
}

void Log::sysdie(const char* msg)
{
	auto tmperno = errno;
    syserr(msg);
	die(strerror(tmperno));
}

Log& Log::instance()
{
	static Log _;
	return _;
}

} /* namespace Util */
