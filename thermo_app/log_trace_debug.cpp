/*
 * log_trace_debug.cpp
 *
 *  Created on: Jan 10, 2015
 *      Author: j
 */

#include <errno.h>
#include <string>
#include <sstream>
#include <time.h>
#include <string.h>
#include <iomanip>
#include <iostream>

namespace ltd
{

static const std::string timestamp()
{
	struct timespec tp;
	struct tm res;
	clock_gettime(CLOCK_REALTIME, &tp);
	gmtime_r(&tp.tv_sec, &res);
	std::stringstream s;
	using std::setw;
	s << std::setfill('0') << setw(2) << res.tm_hour << ':'
			          << setw(2) << res.tm_min << ':'
					  << setw(2) << res.tm_sec << '.'
					  << setw(4) << (tp.tv_nsec/(int)1e5);

	return s.str();
}

void logdebugtrace(const char* file, const int line, const char* msg, bool die, bool syserr, bool outstderr)
{
	const int serrno(errno);

	auto& o = outstderr ? std::cerr : std::cout;

	o << timestamp();
	if(file)
		o << ':' << file;
	if(line)
		o << ':' << line;
	if(msg)
		o << ':' << msg;
	if(syserr)
		o << ':' << strerror(serrno);

	o << '\n';

	if(die)
		exit(EXIT_FAILURE);
}

}
