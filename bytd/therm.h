#pragma once

#include "exporter.h"

namespace ow {
	class OwThermNet;
}

class MeranieTeploty
{
public:
	explicit MeranieTeploty();
	~MeranieTeploty();
	bool init(ow::ExporterSptr exporter);
	void meas();

private:
	std::unique_ptr<ow::OwThermNet> therm;
};
