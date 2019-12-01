/*
 * pgdbwrapper.h
 *
 *  Created on: Jan 8, 2015
 *      Author: j
 */

#ifndef PGDBWRAPPER_H_
#define PGDBWRAPPER_H_

#include <vector>
#include <map>
#include <string>
#include <utility>
#include <libpq-fe.h>
#include "data.h"

class PgDbWrapper
{
public:
	typedef std::pair<std::string, unsigned> SqlCmd;

	explicit PgDbWrapper();
	~PgDbWrapper();

	bool connect();
	bool connectionOK() const;
	bool insertSamples(const SqlCmd& sqlcmd) const;
	SqlCmd createSqlCmd(const std::vector<ow::Sample>& v) const;

private:
	const std::string& encodeRomCode(const ow::RomCode &rc) const;

	PGconn* mConn;
	mutable std::map<uint64_t, std::string> mRomCodeCache;
};

#endif /* PGDBWRAPPER_H_ */
