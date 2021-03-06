/*
 * Pru.h
 *
 *  Created on: Dec 9, 2019
 *      Author: j
 */

#pragma once

#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <chrono>

class PruRxMsg
{
	std::mutex lck;
	std::condition_variable cv;

public:
	constexpr static auto maxlen = 32;
	using Buf = std::vector<uint8_t>;

    template<typename _Rep, typename _Period>
	Buf wait(const std::chrono::duration<_Rep, _Period>& rtime)
	{
    	std::unique_lock<std::mutex> guard(lck);
		cv.wait_for(guard, rtime, [this](){
			return not buf.empty();
		});
		return std::move(buf);
	}

    void rx(Buf buf);

private:
	Buf buf;
};

class Pru {
public:
	Pru();
	void send(const uint8_t* data, std::size_t len);
	void setOwCbk(std::weak_ptr<PruRxMsg> rxMsg)
	{
		owRxMsg = rxMsg;
	}

	void setOtCbk(std::weak_ptr<PruRxMsg> rxMsg)
	{
		otRxMsg = rxMsg;
	}
	~Pru();
private:
	int fd = -1;
	std::thread thrd;
	std::weak_ptr<PruRxMsg> owRxMsg;
	std::weak_ptr<PruRxMsg> otRxMsg;
};
