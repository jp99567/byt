#include "I2cDiOut.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>

#include "Log.h"

void I2CDiOut::i2cwrite(const char* buf, unsigned len)
{
    int rv = ::write(fd, buf, len);
    if(rv == -1){
        LogSYSDIE("I2CDiOut write");
    }
}

void I2CDiOut::i2cwreg(int addr, uint8_t v)
{
    char buf[2];
    buf[0] = addr;
    buf[1] = v;
    i2cwrite(buf, 2);
}

void I2CDiOut::i2cread(char* buf, unsigned len)
{
    int rv = ::read(fd, buf, len);
    if(rv == -1){
        LogSYSDIE("I2CDiOut read");
    }
}

void I2CDiOut::value(const uint8_t v)
{
        i2cwreg(0xA, v);
}

I2CDiOut::I2CDiOut()
{
    fd = ::open("/dev/i2c-2", O_RDWR);

    if(fd < 0) {
        LogSYSDIE("I2CDiOut");
    }

//    constexpr unsigned addr = 0x20;
    int rv = ::ioctl(fd, I2C_SLAVE, 0x20);
    if (rv < 0) {
        LogSYSDIE("I2CDiOut");
    }

    uint8_t iocon = (1<<2) | (1<<5);
    i2cwreg(5, iocon);
    i2cwreg(0xA, 0xFE);
    i2cwreg(0x0, 0x0);

}