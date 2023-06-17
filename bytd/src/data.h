#pragma once

#include <cstdint>
#include <chrono>
#include <vector>
#include <tuple>
#include <string>

namespace ow
{

bool check_crc(const uint8_t* ptr, unsigned size, uint8_t crc);

template< typename T>
bool check_crc(const T& owdata)
{
	auto ptr = reinterpret_cast<const uint8_t*>(&owdata);

	return check_crc(ptr, sizeof(owdata)-1, owdata.crc);
}

#pragma pack(push, 1)
struct RomCode
{
	uint8_t family = 0;
	uint8_t serial[6] = {0};
	uint8_t crc = 0;

    RomCode(){}
    explicit RomCode(const std::string& str)
    {
        *this = str;
    }
	bool operator<(const RomCode& other) const;
	bool operator==(const RomCode& other) const;
	operator std::string() const;
	RomCode& operator=(const std::string& str);
    static bool fromStr(RomCode& rc, const std::string& str);
};
#pragma pack(pop)

class Sample
{
public:
	explicit Sample();
	Sample(Sample&& that);
	Sample& operator=(const Sample&& that);

	Sample(const Sample&) = delete;
	Sample& operator=(const Sample&) = delete;

	void add(const RomCode& rc, const float v);

	const inline std::chrono::system_clock::time_point getTimePoint() const { return t; }
	const inline std::vector<std::tuple<RomCode,float>>& getData() const
		{
			return data;
		}
	inline bool empty(){return data.empty();}

private:
	std::chrono::system_clock::time_point t;
	std::vector<std::tuple<RomCode,float>> data;
};

}

