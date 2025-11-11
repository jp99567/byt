#include "hwif.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "Log.h"

#ifdef BYTD_SIMULATOR
constexpr auto default_can_dev_name = "vcan0";
#else
constexpr auto default_can_dev_name = "can1";
#endif

namespace hwif {

int open_pru()
{
#ifdef BYTD_SIMULATOR
    auto serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un server_addr;
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, "/tmp/pru_sim_socket");
    auto res = connect(serverSocket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(res != 0){
        LogSYSERR("simulator connect pru sim failed");
    }
    return serverSocket;
#else
    return ::open("/dev/rpmsg_pru30", O_RDWR);
#endif
}

std::string can_dev_name()
{
    auto env_devname = ::getenv("BYTD_CAN_DEVICE_NAME");
    if(env_devname)
        return env_devname;
    return default_can_dev_name;
}

std::string mqtt_broker_hostname()
{
    auto env_hostname = ::getenv("BYTD_MQTT_BROKER_HOSTNAME");
    if(env_hostname)
        return env_hostname;
    return "localhost";
}

}
