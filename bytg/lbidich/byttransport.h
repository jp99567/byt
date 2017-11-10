#pragma once

#include <thrift/transport/TTransport.h>
#include "iconnection.h"

using apache::thrift::transport::TTransportException;

class BytTransport : public apache::thrift::transport::TTransport
{
public:

    BytTransport(lbidich::IConnection& conn)
        :connection(conn)
    {}

    virtual bool isOpen()
    {   auto rv = connection.isOpen();
        qDebug() << "BytTransport::isOpen()" << rv;
        return rv;
    }

    virtual void open()
    {
        qDebug() << "BytTransport::open()";
        if(!connection.open())
            throw TTransportException(TTransportException::NOT_OPEN, "Cannot open base TTransport.");
    }

    virtual void close()
    {
        qDebug() << "BytTransport::close()";
        if(!connection.close())
            throw TTransportException(TTransportException::NOT_OPEN, "Cannot close base TTransport.");
    }

    virtual uint32_t read_virt(uint8_t* buf, uint32_t len)
    {
        qDebug() << "BytTransport::read() req:" << len;
       auto size = connection.recv(buf, len);
       qDebug() << "BytTransport::read()" << size << '/' << len;
       if(size == 0)
           throw TTransportException(TTransportException::END_OF_FILE);
       else if(size < 0)
           throw TTransportException(TTransportException::UNKNOWN);
       return size;
    }

    virtual void write_virt(const uint8_t* buf, uint32_t len)
    {
        qDebug() << "BytTransport::write()" << len;
      if(!connection.send(buf, len))
        throw TTransportException(TTransportException::UNKNOWN);
    }

private:
    lbidich::IConnection& connection;
};
