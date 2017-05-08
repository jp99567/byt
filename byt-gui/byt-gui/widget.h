#ifndef WIDGET_H
#define WIDGET_H

#include <memory>
#include <QWidget>
#include <QThread>

class QPushButton;
class QLabel;
class TcpConnection;

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = 0);
    ~Widget();

signals:
    void connectionReq();

private:
    QPushButton* connection;
    QPushButton* audioPowerOn;
    QPushButton* audioCh1;
    QPushButton* audioCh2;
    QLabel * connInfo;
    TcpConnection* bytConn;
    QThread ioThread;

private slots:
    void onConnectionButtonChecked(bool on);
};

#endif // WIDGET_H
