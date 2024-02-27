/*
 * exporter.cpp
 *
 *  Created on: Jan 8, 2015
 *      Author: j
 */

#include "exporter.h"

#include "Log.h"
#include "thread_util.h"
#include <cassert>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

namespace ow {

Exporter::Exporter()
    : mFini(false)
    , mTask(&Exporter::svc, this)
    , _direntry_buffer(new char[offsetof(struct dirent, d_name) + 256])
    , mConnected(false)
    , mLastConnFail(std::chrono::system_clock::time_point::min())
    , mLastBackup(std::chrono::system_clock::now())
    , mFileNameIndex(0)
{
}

Exporter::~Exporter()
{
    mFini = true;
    // close(4); //pgsock
    ::alarm(2);
    mCond.notify_one();
    mTask.join();
    LogINFO("export finish nice");
}

bool Exporter::put(Sample&& /*v*/)
{ /*
         std::unique_lock<std::mutex> lk(mLock);
         mQueue.push(std::move(v));
     mCond.notify_one();*/
    return true;
}

void Exporter::svc()
{
    thread_util::sigblock(true, false);
    thread_util::set_thread_name("bytd-export");

    while(!mFini) {
        std::unique_lock<std::mutex> lk(mLock);

        while(mQueue.empty() && !mFini) {
            mCond.wait(lk);
        }

        if(mQueue.empty()) {
            continue;
        }

        Sample tmp(std::move(mQueue.front()));
        mQueue.pop();

        lk.unlock();
        exportdata(std::move(tmp));
    }

    mLock.lock();
    while(!mQueue.empty()) {
        mToExport.push_back(std::move(mQueue.front()));
        mQueue.pop();
    }
    mLock.unlock();

    if(!mToExport.empty()) {
        backup_data(mToExport);
    }
}

constexpr unsigned max_sample_pending = 1000;
constexpr unsigned offline_backup_interval = 900;
constexpr unsigned reconnect_interval = 60;

void Exporter::exportdata(Sample&& v)
{
    mToExport.push_back(std::move(v));

    using std::chrono::seconds;
    using std::chrono::system_clock;
    if(!mConnected && mLastConnFail + seconds(reconnect_interval) < system_clock::now()) {

        mConnected = mDB.connect();

        if(mConnected) {
            restore_backup();
        } else {
            mLastConnFail = system_clock::now();
        }
    }

    if(mConnected) {
        auto sqlcmd = mDB.createSqlCmd(mToExport);
        if(mDB.insertSamples(sqlcmd)) {
            mToExport.clear();
        } else if(mDB.connectionOK()) {
            store_rejected_data(sqlcmd);
            mToExport.clear();
        } else {
            mConnected = false;
            mLastConnFail = system_clock::now();
        }
    } else if(mLastBackup + seconds(offline_backup_interval) < system_clock::now()) {
        backup_data(mToExport);
        mToExport.clear();
        mLastBackup = system_clock::now();
    }
}

std::string Exporter::filename(const char* base, unsigned count)
{
    struct timespec tp;
    struct tm res;
    clock_gettime(CLOCK_REALTIME, &tp);
    gmtime_r(&tp.tv_sec, &res);
    std::stringstream s;
    using std::setw;

    if(mFileNameIndex > 99)
        mFileNameIndex = 0;
    s << base << '-' << std::setfill('0') << setw(3) << res.tm_yday << '-'
      << setw(2) << res.tm_hour
      << setw(2) << res.tm_min
      << setw(2) << res.tm_sec << '-'
      << setw(2) << mFileNameIndex++;
    if(count) {
        s << '-' << count;
    }

    s << ".sql";

    return s.str();
}

static void save_file(const std::string& filename, const std::string& data)
{
    std::ofstream f;
    f.exceptions(std::ifstream::badbit);
    f.open(filename);
    f << data;
}

static std::string load_file(const std::string& filename)
{
    std::ifstream f;
    f.exceptions(std::ifstream::badbit);
    f.open(filename);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

void Exporter::backup_data(const std::vector<Sample>& v)
{
    auto cmd = mDB.createSqlCmd(v);
    save_file(filename("backup", cmd.second), cmd.first);
}

void Exporter::store_rejected_data(const PgDbWrapper::SqlCmd& cmd)
{
    save_file(filename("rejected"), cmd.first);
}

void Exporter::restore_backup()
{
    auto dirp = ::opendir(".");
    if(!dirp) {
        LogSYSDIE();
    }

    std::vector<std::string> v;
    do {
        errno = 0;
        auto res = readdir(dirp);

        if(not res)
            break;

        if(DT_REG == res->d_type || DT_LNK == res->d_type) {
            // backup-DDD-HHMMSS-II-%d.sql
            const std::string ptr("backup-");
            auto len = ptr.size();
            std::string file(res->d_name);
            if(0 == ptr.compare(0, len, file, 0, len)) {
                v.push_back(std::move(file));
            }
        }
    } while(1);
    ::closedir(dirp);

    for(auto& f : v) {
        // backup-DDD-HHMMSS-II-%d.sql
        unsigned count = std::stoi(f.substr(21));
        auto cmd = load_file(f);
        if(mDB.insertSamples(PgDbWrapper::SqlCmd(cmd, count))) {
            if(unlink(f.c_str()))
                LogSYSDIE();
        } else if(mDB.connectionOK()) {
            if(rename(f.c_str(), filename("rejected").c_str()))
                LogSYSDIE();
        }
    }
}

}
