#pragma once

#include "gen-req/BytRequest.h"
#include "ICore.h"

using apache::thrift::transport::TTransport;

class BytRequest : public doma::BytRequestIf
{
public:
	BytRequest(std::shared_ptr<ICore> core) : core(core){}

	void audioSelectChannel(const int8_t chnum) override
	{
		LogINFO("audioSelectChannel {}", (int)chnum);
		core->sw(chnum,true);
	}

	double ampers() override
	{
		LogINFO("ampers");
		return core->status();
	}

private:
	std::shared_ptr<ICore> core;
};

class BytRequestFactory : public doma::BytRequestIfFactory
{
public:
	explicit BytRequestFactory(std::shared_ptr<ICore> core)
	:core(core){}
	doma::BytRequestIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) override
	{
		  return new BytRequest(core);
	}

	void releaseHandler(doma::BytRequestIf* p) override
	{
		  delete p;
	}

private:
	std::shared_ptr<ICore> core;
};
