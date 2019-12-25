#pragma once

#include <systemd/sd-daemon.h>
#include <iostream>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "thread_util.h"
#include "BytRequest.h"
#include "ZweiKanalServer.h"
#include "exporter.h"
#include "therm.h"
#include "Elektromer.h"
#include "Pru.h"
#include "OpenTherm.h"

class Facade : public ICore
{
public:
        explicit Facade()
        {}

        bool sw(int id, bool on) final
        {
                ++stat;
                return true;
        }

        bool cmd(int id) final
        {
                return true;
        }

        unsigned status()  final
        {
                return stat;
        }

private:
        unsigned stat = 0;
};

class AppContainer
{
        boost::asio::io_service io_service;
        boost::asio::signal_set signals;
        std::unique_ptr<apache::thrift::server::TThreadedServer> server;
        ow::ExporterSptr exporter;
        std::shared_ptr<MeranieTeploty> meranie;
        std::shared_ptr<OpenTherm> openTherm;

public:
        explicit AppContainer();
        void run();
};

