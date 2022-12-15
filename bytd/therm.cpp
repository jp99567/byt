
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
#include "mqtt.h"

MeranieTeploty::~MeranieTeploty()
{
}

MeranieTeploty::MeranieTeploty(std::shared_ptr<Pru> pru, std::vector<std::tuple<std::string, std::string>> reqsensors, MqttClient& mqtt)
    :mqtt(mqtt)
{
		std::set<ow::RomCode> predefined;

        for(const auto& reqsens : reqsensors){
            ow::RomCode rc;
            ow::RomCode::fromStr(rc, std::get<1>(reqsens));
            auto name = std::get<0>(reqsens);
            if(!predefined.insert(rc).second){
                LogDIE("duplicates:{} {}", name, (std::string)rc);
            }
            sensors.emplace_back(rc, std::move(name));
        }

		if(predefined.empty()){
			throw std::runtime_error("no predefined sensors");
		}

        therm = std::make_unique<ow::OwThermNet>(pru);

		bool search_success(false);
		int try_nr(10);
		do{
			std::set<ow::RomCode> found;
			using std::begin;
			using std::end;

            auto found_sensors = therm->search();
			bool goto_break(false);
            for(auto& i: found_sensors){

                if(!found.insert(i).second){
					LogERR("duplicate");
					goto_break = true;
				}
			}
			if(goto_break)break;

			std::vector<ow::RomCode> tmpv(found.size()+predefined.size());

			if(found == predefined){
				search_success = true;
				break;
			}
			else{
				auto it = std::set_difference(begin(predefined), end(predefined),
								       	   	  begin(found), end(found),
											  begin(tmpv));

				if(it!=begin(tmpv)){
					for(auto i=begin(tmpv); i!=it; ++i){
						LogERR("missing sensor: {}", (std::string)*i);
					}
				}

				it = std::set_difference(begin(found), end(found),
										 begin(predefined), end(predefined),
										 begin(tmpv));

				if(it!=begin(tmpv)){
					for(auto i=begin(tmpv); i!=it; ++i){
						LogERR("new sensors: {}", (std::string)*i);
					}
				}
			}
			sleep(1);
		}
		while(--try_nr > 0);

	if(!search_success)
		throw std::runtime_error("search failed");
}

void MeranieTeploty::meas()
{
    therm->convert();
    std::this_thread::sleep_for(std::chrono::milliseconds(750));
    for(auto& sensor : sensors){
        auto v = therm->measure(sensor.romcode);

        if(sensor.update(v)){
            mqtt.publish(std::string("rb/ow/").append(sensor.name), std::to_string(v));
        }
    }
}

bool Sensor::update(float v)
{
    if(v==curVal)
        return false;
    curVal = v;

    if(not std::isnan(v))
        lastValid = v;

    return true;
}
