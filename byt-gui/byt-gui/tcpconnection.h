#pragma once

#include <memory>
#include <QAbstractSocket>

class TcpConnection : public QObject
{
    Q_OBJECT
public:
    TcpConnection();

    ~TcpConnection();

public slots:
    void connectTo();

signals:
    void resultReady(const QString &result);

private:
    std::unique_ptr<QAbstractSocket> socket;

private slots:
    void stateChanged(QAbstractSocket::SocketState state);
};
