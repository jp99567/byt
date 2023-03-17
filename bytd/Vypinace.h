#pragma once

#include "Event.h"
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>

class VypinacDuo
{
public:
    explicit VypinacDuo();
    enum Button { RU, RD, LU, LD};
    enum Combo {L, R};
    enum State { None=-1, U=0, D=1 };
    void pressed(Button id, bool on);
    event::Event<> ClickedRU;
    event::Event<> ClickedRD;
    event::Event<> ClickedLU;
    event::Event<> ClickedLD;
    event::Event<> ClickedBothU;
    event::Event<> ClickedBothD;
    event::Event<> ClickedDIagonalRULD;
    event::Event<> ClickedDIagonalLURD;
private:
    std::array<State, 2> combo;
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

