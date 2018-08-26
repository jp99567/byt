/*
 * pgdbwrapper.cpp
 *
 *  Created on: Jan 8, 2015
 *      Author: j
 */

#include "pgdbwrapper.h"
#include <stdio.h>
#include <cassert>
#include <iostream>
#include <sstream>
#include "Log.h"

static constexpr char conninfo[]  = "connect_timeout = 10";

PgDbWrapper::PgDbWrapper()
:mConn(nullptr)
{
}

PgDbWrapper::~PgDbWrapper()
{
	if(mConn)
		PQfinish(mConn);
}

bool PgDbWrapper::connectionOK() const
{
	return mConn && PQstatus(mConn)==CONNECTION_OK;
}

bool PgDbWrapper::connect()
{
	auto t1 = std::chrono::system_clock::now();
	if(!mConn){
		mConn = PQconnectdb(conninfo);
	}
	else{
		PQreset(mConn);
	}
	auto t2 = std::chrono::system_clock::now();
	LogINFO("DB connection attempt {}usec", std::chrono::duration_cast<std::chrono::seconds>(t2-t1).count());

    /* Check to see that the backend connection was successfully made */
    if (PQstatus(mConn) != CONNECTION_OK)
    {
        LogERR("connect db: {}", PQerrorMessage(mConn));
        return false;
    }
    return true;
}

const std::string& PgDbWrapper::encodeRomCode(const ow::RomCode& rc) const
{
	const uint64_t& irc(reinterpret_cast<const uint64_t&>(rc));

	auto v = mRomCodeCache.find(irc);

	if(v != std::end(mRomCodeCache)){
		return v->second;
	}

	return mRomCodeCache[irc] = "\'x" + (std::string)rc + '\'';
}

PgDbWrapper::SqlCmd PgDbWrapper::createSqlCmd(const std::vector<ow::Sample>& v) const
{
	std::stringstream s;
	using std::get;
	using std::begin;
	using std::end;

	unsigned count(0);
	s << "INSERT INTO temperature VALUES\n";
	for(auto sample = begin(v); sample != end(v); ++sample){
		auto t = std::chrono::system_clock::to_time_t(sample->getTimePoint());
		for( auto i = begin(sample->getData()); i != end(sample->getData()); ++i){
			++count;
			auto comma = sample+1 == end(v) && i+1 == end(sample->getData()) ? ")\n" : "),\n";
			const std::string val = get<1>(*i) == ow::badval ? "NULL" : std::to_string(get<1>(*i));
			s << "(to_timestamp(" << t << "),placeid(" << encodeRomCode(get<0>(*i)) << ")," << val << comma;
		}
	}
	return SqlCmd(s.str(), count);
}

bool PgDbWrapper::insertSamples(const SqlCmd& sqlcmd) const
{
	assert(mConn);

	const std::string& cmdstr = sqlcmd.first;
	const int count = sqlcmd.second;

	bool rv(true);
	auto t1 = std::chrono::system_clock::now();
    PGresult* res = PQexec(mConn, cmdstr.c_str());
    auto t2 = std::chrono::system_clock::now();
    if(PQresultStatus(res) != PGRES_COMMAND_OK)
    {
    	rv = false;
    	auto dur = std::chrono::duration_cast<std::chrono::seconds>(t2-t1).count();
        LogERR("INSERT failed: {} {} {}usec", PQresultStatus(res), PQerrorMessage(mConn), dur);
    }
    else if(std::stoi(PQcmdTuples(res)) != count){
    	rv = false;
    	LogERR("not expected inserted count: {}", PQcmdTuples(res));
    }
    PQclear(res);

	return rv;
}
