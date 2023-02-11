#pragma once

struct can_frame;

class CanBus
{
    int socket = -1;
public:
    CanBus();
    ~CanBus();
private:
    void read(can_frame& frame) const;
    void write(const can_frame& frame) const;
};

