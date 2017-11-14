#pragma

class ICore
{
  public:
	virtual ~ICore(){}
	virtual bool sw(int id, bool on){}
	virtual bool cmd(int id){}
	virtual unsigned status() const = 0;
};
