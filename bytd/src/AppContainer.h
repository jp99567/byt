#pragma once


#include <systemd/sd-daemon.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>

#include "exporter.h"
#include "OpenTherm.h"
#include <boost/asio.hpp>
#include "mqtt.h"
#include "slowswi2cpwm.h"
#include "Pumpa.h"
#include <gpiod.hpp>
#include "canbus.h"


class AppContainer
{
    boost::asio::io_service io_service;
    boost::asio::signal_set signals;
    boost::asio::steady_timer t1sec;
    boost::asio::steady_timer timerKurenie;
    CanBus canBus;
    ow::ExporterSptr exporter;
    std::shared_ptr<OpenTherm> openTherm;
    std::shared_ptr<MqttClient> mqtt;
    std::unique_ptr<slowswi2cpwm> slovpwm;

public:
    explicit AppContainer();
    void run();

 private:

    void on1sec();
    void sched_1sect();
    void doRequest(std::string msg);
};
