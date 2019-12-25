
#include "Log.h"
#include "AppContainer.h"

int main()
{
  Util::LogExit scopedLog("GRACEFUL");

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
