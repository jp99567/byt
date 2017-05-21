#pragma once

#include <QObject>
#include <QDebug>
#include <memory>

#include <thrift/protocol/TBinaryProtocol.h>
//#include <thrift/transport/TSocket.h>
//#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TTransportUtils.h>

#include "gen-req/BytRequest.h"
#include "tcpconnection.h"

class BytRequest : public QObject
{
    Q_OBJECT
public:
    BytRequest(boost::shared_ptr<apache::thrift::transport::TTransport> clientChannel, QObject* parent = nullptr)
        :QObject(parent)
    {
        using apache::thrift::transport::TTransport;
        using apache::thrift::transport::TBufferedTransport;
        using apache::thrift::protocol::TBinaryProtocol;

        boost::shared_ptr<TTransport> transport(new TBufferedTransport(clientChannel));
        boost::shared_ptr<apache::thrift::protocol::TProtocol> protocol(new TBinaryProtocol(transport));
        client = std::unique_ptr<doma::BytRequestClient>(new doma::BytRequestClient(protocol));
    }

    ~BytRequest()
    {
        qDebug() << "destroy" << "~BytRequest";
    }

public slots:
    void c1()
    {
        qDebug() << "c1";
        if(client)
        {
            try{
                client->audioSelectChannel(1);
            }
            catch(apache::thrift::TException e){
                qDebug() << "c1 catch" << e.what();
            }
        }
    }

    void c2()
    {
        qDebug() << "c2";
        if(client)
        {
            try{
                auto val = client->ampers();
                qDebug() << "c2 val" << val;
            }
            catch(apache::thrift::TException e){
                qDebug() << "c2 catch" << e.what();
            }
        }
    }

private:
    std::unique_ptr<doma::BytRequestClient> client;
};
