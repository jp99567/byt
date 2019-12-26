#pragma once

#include "ICore.h"

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

class FacadeAtrapa : public ICore
{
public:
        explicit FacadeAtrapa()
        {}

        bool sw(int id, bool on) final
        {
                ++stat;
                return true;
        }

        bool cmd(int id) final
        {
                return true;
        }

        unsigned status()  final
        {
                return stat;
        }

private:
        unsigned stat = 0;
};

class OpenTherm;
class MeranieTeploty;

class Facade : public ICore
{
    std::unique_ptr<MeranieTeploty> meranie;
    std::unique_ptr<OpenTherm> openTherm;

    bool running = true;
    std::mutex cvlock;
    std::condition_variable cv_running;
    std::thread measthread;

public:
    Facade();
    ~Facade();

    bool sw(int, bool) override { return false; }
    bool cmd(int) override { return false;}
    unsigned status() override {return 0;}
};
