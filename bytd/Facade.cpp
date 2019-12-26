#include "Facade.h"

#include <fstream>

#include "thread_util.h"
#include "exporter.h"
#include "therm.h"
#include "Elektromer.h"
#include "Pru.h"
#include "OpenTherm.h"

Facade::Facade()
{
    auto exporter = std::make_shared<ow::Exporter>();
    auto pru = std::make_shared<Pru>();
    meranie = std::make_unique<MeranieTeploty>(pru, exporter);
    openTherm = std::make_unique<OpenTherm>(std::move(pru));

    measthread = std::thread([this]{
        thread_util::set_thread_name("bytd-meranie");

        Elektromer em;
        do{
            meranie->meas();
            std::unique_lock<std::mutex> ulck(cvlock);
            cv_running.wait_for(ulck, std::chrono::seconds(10), [this]{
                return !running;
            });

            {
                std::ifstream fdhw("/run/bytd/setpoint_dhw");
                fdhw >> openTherm->dhwSetpoint;
            }
            {
                std::ifstream fch("/run/bytd/setpoint_ch");
                fch >> openTherm->chSetpoint;
            }

        }
        while(running);
    });
}

Facade::~Facade()
{
    {
        std::unique_lock<std::mutex> guard(cvlock);
        running = false;
        cv_running.notify_all();
    }
    measthread.join();
}
