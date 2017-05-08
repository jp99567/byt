#pragma once

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/transport/TTransport.h>

#include "gen-req/BytRequest.h"

class IConnection
{
public:
    virtual bool isOpen() const = 0;
    virtual bool peek() const = 0;
    virtual bool send(const uint8_t* data, unsigned size) = 0;
    virtual int recv(uint8_t* data, unsigned size) = 0;
    virtual bool open() = 0;
    virtual bool close() = 0;
    virtual ~IConnection(){}
};

using apache::thrift::transport::TTransportException;

class BytTransport : public apache::thrift::transport::TTransport
{
public:

    BytTransport(IConnection& conn)
        :connection(conn)
    {}

    virtual bool isOpen()
    {return connection.isOpen(); }

    virtual bool peek()
    {
        return connection.peek(); }

    virtual void open()
    {
        if(!connection.open())
            throw TTransportException(TTransportException::NOT_OPEN, "Cannot open base TTransport.");
    }

    virtual void close()
    {
        if(!connection.close())
            throw TTransportException(TTransportException::NOT_OPEN, "Cannot close base TTransport.");
    }

    virtual uint32_t read_virt(uint8_t* buf, uint32_t len)
    {
       auto size = connection.recv(buf, len);
       if(size == 0)
           throw TTransportException(TTransportException::END_OF_FILE);
       else if(size < 0)
           throw TTransportException(TTransportException::UNKNOWN);
    }

    virtual void write_virt(const uint8_t* buf, uint32_t len)
    {
      if(!connection.send(buf, len))
        throw TTransportException(TTransportException::UNKNOWN);
    }

private:
    IConnection& connection;
};
