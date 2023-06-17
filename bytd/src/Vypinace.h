#pragma once

#include "Event.h"
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>

class VypinacDuo
{
public:
    enum Button { RU, RD, LU, LD};
    enum Combo {L, R};
    enum State { None=-1, U=0, D=1 };
    event::Event<> ClickedRU;
    event::Event<> ClickedRD;
    event::Event<> ClickedLU;
    event::Event<> ClickedLD;
    event::Event<> ClickedBothU;
    event::Event<> ClickedBothD;
    event::Event<> ClickedDIagonalRULD;
    event::Event<> ClickedDIagonalLURD;
    virtual void pressed(Button id, bool on) = 0;
    virtual ~VypinacDuo(){}
protected:
    std::array<State, 2> combo;
};

class VypinacDuoSimple : public VypinacDuo
{
public:
    explicit VypinacDuoSimple();
    void pressed(Button id, bool on) override;
};

class VypinacSingle
{
public:
    explicit VypinacSingle(boost::asio::io_service& io_context);
    enum Button { U, D};
    void pressed(Button id, bool on);
    event::Event<> ClickedU;
    event::Event<> ClickedD;
    event::Event<> ClickedLongU;
    event::Event<> ClickedLongD;
 private:
    std::array<bool, 2> button;
    boost::asio::steady_timer timer;
    bool longPress = false;
    void onTimer();
    void notifyClicked(Button id);
};

class VypinacDuoWithLongPress : public VypinacDuo
{
public:
    explicit VypinacDuoWithLongPress(boost::asio::io_service& io_context);
    void pressed(Button id, bool on) override;
    event::Event<> ClickedLongRU;
    event::Event<> ClickedLongRD;
    event::Event<> ClickedLongLU;
    event::Event<> ClickedLongLD;
    event::Event<> ClickedLongBothU;
    event::Event<> ClickedLongBothD;
    event::Event<> ClickedLongDIagonalRULD;
    event::Event<> ClickedLongDIagonalLURD;
private:
    boost::asio::steady_timer timer;
    bool longPress = false;
    void onTimer();
};
