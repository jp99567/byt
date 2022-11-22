#pragma once

#include <cstdint>

class I2CDiOut
{
    int fd = -1;

public:
    I2CDiOut();
    void value(const uint8_t v);
private:
    void i2cwrite(const char* buf, unsigned len);
    void i2cwreg(int addr, uint8_t v);
    void i2cread(char* buf, unsigned len);
};
