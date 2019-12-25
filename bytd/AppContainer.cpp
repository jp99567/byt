#include "AppContainer.h"

#include "Log.h"

AppContainer::AppContainer()
        :signals(io_service, SIGINT, SIGTERM)
{
    server = std::make_unique<apache::thrift::server::TThreadedServer>(std::make_shared<doma::BytRequestProcessorFactory>(std::make_shared<BytRequestFactory>(std::make_shared<Facade>())),
                    std::make_shared<ZweiKanalServer>(io_service),
                std::make_shared<apache::thrift::transport::TBufferedTransportFactory>(),
                std::make_shared<apache::thrift::protocol::TBinaryProtocolFactory>());

    signals.async_wait([this](auto error, auto signum){
        LogINFO("signal {}",signum);
        server->stop();
    });

    exporter = std::make_shared<ow::Exporter>();
    auto pru = std::make_shared<Pru>();
    meranie = std::make_shared<MeranieTeploty>(pru, exporter);
    openTherm = std::make_shared<OpenTherm>(pru);
}


void AppContainer::run()
{
    bool running = true;
    std::mutex cvlock;
    std::condition_variable cv_running;

    std::thread meastempthread([this,&running,&cv_running,&cvlock]{
        thread_util::set_thread_name("bytd-meranie");

        Elektromer em;
        do{
            meranie->meas();
            std::unique_lock<std::mutex> ulck(cvlock);
            cv_running.wait_for(ulck, std::chrono::seconds(10), [&running]{
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

    sd_notify(0, "READY=1");

    server->serve();
    LogINFO("thrift server exited");

    {
        std::unique_lock<std::mutex> guard(cvlock);
        running = false;
        cv_running.notify_all();
    }
    meastempthread.join();
}
