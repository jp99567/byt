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

signals:
    void newVal(float v);

public slots:
    void c1Pressed()
    {
        c1(true);
    }
    void c1Released()
    {
        c1(false);
    }
    void c2();

private:
    void c1(bool on);
    void buildClient();
    TcpConnection& tcp;
    std::unique_ptr<doma::BytRequestIf> client;
};
