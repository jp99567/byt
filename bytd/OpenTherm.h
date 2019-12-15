/*
 * OpenTherm.h
 *
 *  Created on: Dec 12, 2019
 *      Author: j
 */

#pragma once

#include <memory>
#include <thread>

class Pru;
class PruRxMsg;

class OpenTherm
{
public:
	explicit OpenTherm(std::shared_ptr<Pru> pru);
	~OpenTherm();

	float dhwSetpoint = 38;
	float chSetpoint = 0;

private:
	uint32_t transmit(uint32_t frame);
	std::thread thrd;
	bool shutdown = false;
	std::shared_ptr<Pru> pru;
	std::shared_ptr<PruRxMsg> rxMsg;
};
