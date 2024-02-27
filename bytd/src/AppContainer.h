#pragma once

#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <systemd/sd-daemon.h>
#include <thread>

#include "OpenTherm.h"
#include "Pumpa.h"
#include "builder.h"
#include "canbus.h"
#include "exporter.h"
#include "mqtt.h"
#include "slowswi2cpwm.h"
#include <boost/asio.hpp>
#include <gpiod.hpp>

class AppContainer {
    boost::asio::io_service io_service;
    boost::asio::signal_set signals;
    boost::asio::steady_timer t1sec;
    boost::asio::steady_timer timerKurenie;
    CanBus canBus;
    ow::ExporterSptr exporter;
    std::shared_ptr<OpenTherm> openTherm;
    std::shared_ptr<MqttClient> mqtt;
    std::unique_ptr<slowswi2cpwm> slovpwm;
    AppComponents components;

public:
    explicit AppContainer();
    ~AppContainer();
    void run();

private:
    void on1sec();
    void sched_1sect();
    void sched_kurenie();
    std::string doRequest(std::string msg);
};
