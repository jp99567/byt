
#include "AppContainer.h"
#include "Log.h"

class LogExit {
    const std::string msg;

public:
    LogExit(std::string msg)
        : msg(std::move(msg))
    {
    }
    ~LogExit()
    {
        Util::Log::instance().get()->info("exit:{}", msg);
    }
};

int main(int argc, char* argv[])
{
    if(argc > 1)
    {
        std::string arg1(argv[1]);
        if(arg1 == "-v" || arg1 == "--version"){
            std::cout << AppContainer::version() << std::endl;
            return EXIT_SUCCESS;
        }
        return EXIT_FAILURE;
    }

    LogExit scopedLog("GRACEFUL");
    MqttWrapper libmMosquitto;

    if(not ::getenv("TERM")) {
        Util::Log::instance().get()->set_pattern("[%S.%e][%L] %v");
        Util::Log::instance().get()->set_level(spdlog::level::info);
    }

    try {
        AppContainer().run();
    } catch(std::exception& e) {
        LogERR("app crash: {}", e.what());
        return 1;
    } catch(...) {
        LogERR("app crash: unknown");
        return 1;
    }

    return 0;
}
