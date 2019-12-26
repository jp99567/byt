#pragma once

#include <boost/asio.hpp>

namespace  apache::thrift::server { class TThreadedServer; }

class AppContainer
{
        boost::asio::io_service io_service;
        boost::asio::signal_set signals;
        std::unique_ptr<apache::thrift::server::TThreadedServer> server;

public:
        explicit AppContainer();
        void run();
        ~AppContainer();
};

