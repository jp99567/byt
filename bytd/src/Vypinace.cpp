#include "Vypinace.h"

VypinacDuoSimple::VypinacDuoSimple()
{
    combo.fill(State::None);
}

void VypinacDuoSimple::pressed(Button id, bool on)
{
    if(on) {
        if(id == Button::LD || id == Button::LU) {
            combo[Combo::L] = id == Button::LD ? State::D : State::U;
        } else {
            combo[Combo::R] = id == Button::RD ? State::D : State::U;
        }
    } else {
        if(std::all_of(std::cbegin(combo), std::cend(combo), [](auto b) { return b != State::None; })) {
            if(combo[Combo::L] == State::U && combo[Combo::R] == State::U)
                ClickedBothU.notify();
            else if(combo[Combo::L] == State::D && combo[Combo::R] == State::D)
                ClickedBothD.notify();
            else if(combo[Combo::L] == State::D && combo[Combo::R] == State::U)
                ClickedDIagonalRULD.notify();
            else if(combo[Combo::L] == State::U && combo[Combo::R] == State::D)
                ClickedDIagonalLURD.notify();
        } else if(std::any_of(std::cbegin(combo), std::cend(combo), [](auto b) { return b != State::None; })) {
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

VypinacSingle::VypinacSingle(boost::asio::io_service& io_context)
    : timer(io_context)
{
    button.fill(false);
}

void VypinacSingle::pressed(Button id, bool on)
{
    if(on) {
        timer.expires_at(std::chrono::steady_clock::now() + std::chrono::milliseconds(500));
        timer.async_wait([this](boost::system::error_code ec) {
            longPress = !ec;
            onTimer();
        });
        button[id] = true;
    } else {
        timer.cancel();
    }
}

void VypinacSingle::onTimer()
{
    auto b1 = std::find(std::begin(button), std::end(button), true);
    if(b1 != std::cend(button)) {
        *b1 = false;
        auto id = (Button)(b1 - std::cbegin(button));
        event::Event<>* event[2][2] = { { &ClickedU, &ClickedLongU }, { &ClickedD, &ClickedLongD } };
        event[id][(int)longPress]->notify();
    }
}

VypinacDuoWithLongPress::VypinacDuoWithLongPress(boost::asio::io_service& io_context)
    : timer(io_context)
{
    combo.fill(State::None);
}

void VypinacDuoWithLongPress::pressed(Button id, bool on)
{
    if(on) {
        if(std::all_of(std::cbegin(combo), std::cend(combo), [](auto b) { return b == State::None; })) {
            timer.expires_at(std::chrono::steady_clock::now() + std::chrono::milliseconds(500));
            timer.async_wait([this](boost::system::error_code ec) {
                longPress = !ec;
                onTimer();
            });
        }

        if(id == Button::LD || id == Button::LU) {
            combo[Combo::L] = id == Button::LD ? State::D : State::U;
        } else {
            combo[Combo::R] = id == Button::RD ? State::D : State::U;
        }
    } else {
        timer.cancel();
    }
}

void VypinacDuoWithLongPress::onTimer()
{
    if(std::all_of(std::cbegin(combo), std::cend(combo), [](auto b) { return b != State::None; })) {
        if(combo[Combo::L] == State::U && combo[Combo::R] == State::U) {
            if(longPress)
                ClickedLongBothU.notify();
            else
                ClickedBothU.notify();
        } else if(combo[Combo::L] == State::D && combo[Combo::R] == State::D) {
            if(longPress)
                ClickedLongBothD.notify();
            else
                ClickedBothD.notify();
        } else if(combo[Combo::L] == State::D && combo[Combo::R] == State::U) {
            if(longPress)
                ClickedLongDIagonalRULD.notify();
            else
                ClickedDIagonalRULD.notify();
        } else if(combo[Combo::L] == State::U && combo[Combo::R] == State::D) {
            if(longPress)
                ClickedLongDIagonalLURD.notify();
            else
                ClickedDIagonalLURD.notify();
        }
    } else if(std::any_of(std::cbegin(combo), std::cend(combo), [](auto b) { return b != State::None; })) {
        if(combo[Combo::L] == State::U) {
            if(longPress)
                ClickedLongLU.notify();
            else
                ClickedLU.notify();
        } else if(combo[Combo::L] == State::D) {
            if(longPress)
                ClickedLongLD.notify();
            else
                ClickedLD.notify();
        } else if(combo[Combo::R] == State::U) {
            if(longPress)
                ClickedLongRU.notify();
            else
                ClickedRU.notify();
        } else if(combo[Combo::R] == State::D) {
            if(longPress)
                ClickedLongRD.notify();
            else
                ClickedRD.notify();
        }
    }
    combo.fill(State::None);
}
