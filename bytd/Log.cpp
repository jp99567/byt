/*
 * Log.cpp
 *
 *  Created on: May 21, 2017
 *      Author: j
 */

#include "Log.h"

namespace Util {

Log::~Log() {
}
Log::Log() {
	logger = spdlog::stdout_color_mt("console");

}

Log& Log::instance()
{
	static Log _;
	return _;
}

} /* namespace Util */
