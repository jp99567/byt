
#include "AppContainer.h"
#include "Log.h"

int main()
{
  Util::LogExit scopedLog("GRACEFUL");
  MqttWrapper libmMosquitto;

  if(not ::getenv("TERM")){
      Util::Log::instance().get()->set_pattern("[%S.%e][%L] %v");
      Util::Log::instance().get()->set_level(spdlog::level::info);
  }

  try
  {
      AppContainer().run();
  }
  catch (std::exception& e)
  {
      LogERR("app crash: {}", e.what());
      return 1;
  }
  catch (...)
  {
       LogERR("app crash: unknown");
       return 1;
  }

  return 0;
}
