/*
 * OpenTherm.h
 *
 *  Created on: Dec 12, 2019
 *      Author: j
 */

#pragma once

#include <memory>
#include <thread>
#include <vector>

class Pru;
class PruRxMsg;
class MqttClient;

struct OtParam
{
    int id;
    uint16_t val;
    bool updated=false;
};

struct OtWrParam : OtParam
{
    uint16_t val_mirrored;
};

class OpenTherm
{
public:
    explicit OpenTherm(std::shared_ptr<Pru> pru, MqttClient& mqtt);
	~OpenTherm();

	float dhwSetpoint = 38;
	float chSetpoint = 0;

private:
    void publish_status(uint16_t newstat);
	uint32_t transmit(uint32_t frame);
	std::thread thrd;
	bool shutdown = false;
	std::shared_ptr<Pru> pru;
	std::shared_ptr<PruRxMsg> rxMsg;
	std::vector<OtParam> rdParams;
	std::vector<OtWrParam> wrParams;
    MqttClient& mqtt;
    uint16_t status = 0;
};
