#pragma once

#include "exporter.h"

namespace ow {
	class OwThermNet;
}

class Pru;

class MeranieTeploty
{
public:
	explicit MeranieTeploty(std::shared_ptr<Pru> pru, ow::ExporterSptr exporter);
	~MeranieTeploty();
	void meas();

private:
	std::unique_ptr<ow::OwThermNet> therm;
};
