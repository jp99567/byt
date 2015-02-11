/*
 * exporter.h
 *
 *  Created on: Jan 8, 2015
 *      Author: j
 */

#ifndef EXPORTER_H_
#define EXPORTER_H_

#include <queue>
#include <condition_variable>
#include <thread>
#include <vector>
#include <memory>

#include "pgdbwrapper.h"

namespace ow
{

class Exporter
{
public:
	explicit Exporter();
	~Exporter();

	bool put(Sample&& v);

private:
	bool mFini;
	std::queue<Sample> mQueue;
	std::vector<Sample> mToExport;
	std::thread mTask;
	std::mutex mLock;
	std::condition_variable mCond;
	PgDbWrapper mDB;
	std::unique_ptr<char[]> _direntry_buffer;

	bool mConnected;
	std::chrono::system_clock::time_point mLastConnFail;
	std::chrono::system_clock::time_point mLastBackup;

	void svc();
	void exportdata(Sample&& v);
	void backup_data(const std::vector<Sample>& v);
	void restore_backup();
	void store_rejected_data(const PgDbWrapper::SqlCmd& cmd);
	std::string filename(const char* base, unsigned count=0);
	unsigned mFileNameIndex;
};

}

#endif /* EXPORTER_H_ */
