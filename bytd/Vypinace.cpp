#include "Vypinace.h"
#include "Log.h"

VypinacDuo::VypinacDuo()
{
    combo.fill(VypinacDuo::None);
}

void VypinacDuo::pressed(Button id, bool on)
{
    if(on){
        if( id == Button::LD || id == Button::LU ){
            combo[Combo::L] = id == Button::LD ? State::D : State::U;
        }
        else{
            combo[Combo::R] = id == Button::RD ? State::D : State::U;
        }
    }
    else{
        if(std::all_of(std::cbegin(combo), std::cend(combo), [](auto b){ return b != State::None;})){
            if(combo[Combo::L] == State::U && combo[Combo::R] == State::U)
                ClickedBothU.notify();
            else if(combo[Combo::L] == State::D && combo[Combo::R] == State::D)
                ClickedBothD.notify();
            else if(combo[Combo::L] == State::D && combo[Combo::R] == State::U)
                ClickedDIagonalRULD.notify();
            else if(combo[Combo::L] == State::U && combo[Combo::R] == State::D)
                ClickedDIagonalLURD.notify();
        }
        else if(std::any_of(std::cbegin(combo), std::cend(combo), [](auto b){ return b != State::None;})){
            if(combo[Combo::L] == State::U)
                ClickedLU.notify();
            else if(combo[Combo::L] == State::D)
                ClickedLD.notify();
            else if(combo[Combo::R] == State::U)
                ClickedRU.notify();
            else if(combo[Combo::R] == State::D)
                ClickedRD.notify();
        }
        combo.fill(State::None);
    }
}

VypinacSingle::VypinacSingle(boost::asio::io_service &io_context)
    :timer(io_context)
{
    button.fill(false);
}

void VypinacSingle::pressed(Button id, bool on)
{
    if(on){
        timer.expires_at(std::chrono::steady_clock::now() + std::chrono::milliseconds(500));
        timer.async_wait([this](boost::system::error_code ec){
            longPress = !ec;
            onTimer();
        });
        button[id] = true;
    }
    else{
        timer.cancel();
    }
}

void VypinacSingle::onTimer()
{
    auto b1 = std::find(std::begin(button), std::end(button), true);
    if(b1 != std::cend(button)){
        *b1 = false;
        auto id = (Button)(b1-std::cbegin(button));
        event::Event<>* event[2][2] = {{&ClickedU, &ClickedLongU},{&ClickedD, &ClickedLongD}};
        event[id][(int)longPress]->notify();
    }
}

#include <iostream>
#include <thread>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>

struct TestApp
{
    boost::asio::io_service io_context;
    VypinacDuo vypinac;
    boost::asio::streambuf buf;
    boost::asio::posix::stream_descriptor input;
    std::string line, last;

    TestApp()
        :input(io_context, ::dup(STDIN_FILENO))
    {
        vypinac.ClickedLD.subscribe(event::subscr([]{
            LogINFO("EVENT LD");
        }));
        vypinac.ClickedLU.subscribe(event::subscr([]{
            LogINFO("EVENT LU");
        }));
        vypinac.ClickedRD.subscribe(event::subscr([]{
            LogINFO("EVENT RD");
        }));
        vypinac.ClickedRU.subscribe(event::subscr([]{
            LogINFO("EVENT RU");
        }));
        vypinac.ClickedBothU.subscribe(event::subscr([]{
            LogINFO("EVENT BothU");
        }));
        vypinac.ClickedBothD.subscribe(event::subscr([]{
            LogINFO("EVENT BothD");
        }));
        vypinac.ClickedDIagonalRULD.subscribe(event::subscr([]{
            LogINFO("EVENT DIagonalRULD");
        }));
        vypinac.ClickedDIagonalLURD.subscribe(event::subscr([]{
            LogINFO("EVENT DIagonalLURD");
        }));
    }
    int run_testapp();
    void do_line();
    void readline();
};

void TestApp::readline()
{
    boost::asio::async_read_until(input, buf, '\n', [this](boost::system::error_code ec, std::size_t){
        if(ec){
            LogERR("readline {}", ec.message());
            io_context.stop();
            return;
        }
        std::istream is(&buf);
        std::getline(is, line);
        do_line();
        readline();
    });

}

int TestApp::run_testapp()
{
    readline();
    io_context.run();
    return 0;
}

void TestApp::do_line()
{
        if(last != line){
            last = line;
            if(line == "key press   24"){
                io_context.stop();
            }
            else if(line == "key press   31 "){
                vypinac.pressed(VypinacDuo::LU, true);
            }
            else if(line == "key release 31 "){
                vypinac.pressed(VypinacDuo::LU, false);
            }
            if(line == "key press   32 "){
                vypinac.pressed(VypinacDuo::RU, true);
            }
            else if(line == "key release 32 "){
                vypinac.pressed(VypinacDuo::RU, false);
            }
            if(line == "key press   45 "){
                vypinac.pressed(VypinacDuo::LD, true);
            }
            else if(line == "key release 45 "){
                vypinac.pressed(VypinacDuo::LD, false);
            }
            if(line == "key press   46 "){
                vypinac.pressed(VypinacDuo::RD, true);
            }
            else if(line == "key release 46 "){
                vypinac.pressed(VypinacDuo::RD, false);
            }
        }
}

#ifdef TESTAPP
int main(){
    return TestApp().run_testapp();
}
#endif
