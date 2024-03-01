#pragma once

#include <stdint.h>
#include <avr/io.h>

struct ClockTimer65MS
{
	static uint8_t now()
	{
		return CANTIMH;
	}

	template<unsigned ms>
	static constexpr uint8_t durMs()
	{
		constexpr float rv = ((float)ms*1000)/(256L*256L);
		static_assert(rv<255, "Invalid timeout moc velky");
		static_assert(rv>0, "Invalid timeout moc maly");
		return rv;
	}

	static bool isTimeout(uint8_t t0, uint8_t dur)
	{
		uint8_t dt = now() - t0;
		return dt > dur;
	}
};

