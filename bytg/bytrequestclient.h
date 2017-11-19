#pragma once

#include <QObject>
#include <memory>

namespace doma { class BytRequestIf; }
class TcpConnection;

class BytRequest : public QObject
{
    Q_OBJECT
public:
    BytRequest(TcpConnection& tcp, QObject* parent = nullptr);

    ~BytRequest();

public slots:
    void c1();
    void c2();

private:
    void buildClient();
    TcpConnection& tcp;
    std::unique_ptr<doma::BytRequestIf> client;
};
