#include "AppContainer.h"

#include <systemd/sd-daemon.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>

#include "BytRequest.h"
#include "ZweiKanalServer.h"

#include "Log.h"
#include "Facade.h"

AppContainer::AppContainer()
:signals(io_service, SIGINT, SIGTERM)
{
    auto zweiKanalServer = std::make_shared<ZweiKanalServer>(io_service);

    server = std::make_unique<apache::thrift::server::TThreadedServer>(
                std::make_shared<doma::BytRequestProcessorFactory>(
                    std::make_shared<BytRequestFactory>(std::make_shared<Facade>())),
                zweiKanalServer,
                std::make_shared<apache::thrift::transport::TBufferedTransportFactory>(),
                std::make_shared<apache::thrift::protocol::TBinaryProtocolFactory>());

    signals.async_wait([this](auto error, auto signum){
        LogINFO("signal {}",signum);
        server->stop();
    });
}


void AppContainer::run()
{
    sd_notify(0, "READY=1");

    server->serve();
    LogINFO("thrift server exited");
}

AppContainer::~AppContainer()
{

}
