#include "bytrequestclient.h"

#include <QDebug>
#include <thrift/protocol/TBinaryProtocol.h>
//#include <thrift/transport/TSocket.h>
//#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TTransportUtils.h>

#include "gen-req/BytRequest.h"
#include "tcpconnection.h"

BytRequest::BytRequest(TcpConnection &tcp, QObject *parent)
    :QObject(parent)
    ,tcp(tcp)
{
}

BytRequest::~BytRequest()
{
    qDebug() << "destroy" << "~BytRequest";
}

void BytRequest::c1()
{
    qDebug() << "c1";
    buildClient();
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

void BytRequest::c2()
{
    qDebug() << "c2";
    buildClient();
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
    else{

    }
}

void BytRequest::buildClient()
{
    if(client)
        return;

    using apache::thrift::transport::TTransport;
    using apache::thrift::transport::TBufferedTransport;
    using apache::thrift::protocol::TBinaryProtocol;

    std::shared_ptr<TTransport> transport(new TBufferedTransport(tcp.getClientChannel()));
    std::shared_ptr<apache::thrift::protocol::TProtocol> protocol(new TBinaryProtocol(transport));
    client = std::make_unique<doma::BytRequestClient>(protocol);
}

