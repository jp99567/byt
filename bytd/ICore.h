#pragma once

class ICore
{
  public:
	virtual ~ICore(){}
	virtual bool sw(int id, bool on)=0;
	virtual bool cmd(int id)=0;
	virtual unsigned status() = 0;
};
