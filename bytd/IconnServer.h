#pragma once

#include "TcpConnection.h"

class IConnServer
{
public:
	virtual void addVerified(tcp_connection::sPtr connection) = 0;
	virtual ~IConnServer(){}
};
