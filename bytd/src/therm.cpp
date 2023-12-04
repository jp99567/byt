
#include "therm.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cstdint>
#include <system_error>
#include <vector>
#include <limits>
#include <algorithm>
#include <cstring>
#include <set>
#include <fstream>
#include "Log.h"
#include "Pru.h"
#include "OwThermNet.h"
#include "IMqttPublisher.h"
#include "avr/fw/OwResponseCode_generated.h"

MeranieTeploty::~MeranieTeploty()
{
}

MeranieTeploty::MeranieTeploty(std::shared_ptr<Pru> pru, ow::SensorList reqsensors, MqttClient& mqtt)
    :sensors(std::move(reqsensors))
    ,mqtt(mqtt)
{
    std::set<ow::RomCode> predefined;

    for(const auto& reqsens : sensors){
        if(!predefined.insert(reqsens.first).second){
            LogDIE("duplicates:{} {}", reqsens.second->name(), (std::string)reqsens.first);
        }
    }

    if(predefined.empty()){
        throw std::runtime_error("no predefined sensors");
    }

    therm = std::make_unique<ow::OwThermNet>(pru);

    std::set<ow::RomCode> found;
    using std::begin;
    using std::end;

    auto found_sensors = therm->search();
    for(auto& i: found_sensors){

        if(!found.insert(i).second){
            LogERR("duplicate");
            throw std::runtime_error("search failed");
        }
    }

    std::vector<ow::RomCode> tmpv(found.size()+predefined.size());

    if(found != predefined){
        auto it = std::set_difference(begin(found), end(found),
                                 begin(predefined), end(predefined),
                                 begin(tmpv));

        if(it!=begin(tmpv)){
            for(auto i=begin(tmpv); i!=it; ++i){
                LogERR("new sensors: {}", (std::string)*i);
            }
        }

        it = std::set_difference(begin(predefined), end(predefined),
                                      begin(found), end(found),
                                      begin(tmpv));

        if(it!=begin(tmpv)){
            for(auto i=begin(tmpv); i!=it; ++i){
                LogERR("missing sensor: {}", (std::string)*i);
            }
            throw std::runtime_error("missing ow sensors");
        }
    }
}

void MeranieTeploty::meas()
{
    therm->convert();
    std::this_thread::sleep_for(std::chrono::milliseconds(750));
    for(auto& sensor : sensors){
        auto v = therm->readMeasured(sensor.first);

        if (v != ow::OwThermNet::readScratchPadFailed) {
            sensor.second->setValue(v);
        }
        else{
            sensor.second->setValue(ow::cInvalidValue);
        }
    }
}
