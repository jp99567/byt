#pragma once
#include <string>

namespace hwif {
int open_pru();
std::string can_dev_name();
std::string mqtt_broker_hostname();
}
