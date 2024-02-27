#include "Reku.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <termios.h>
#include <unistd.h>
#ifdef REKU_TESTAPP
#include <fstream>
#endif
#include "Log.h"
#include "data.h"
#include "rekuperacia_fw/reku_control_proto.h"

double adc_temp(uint16_t adc)
{
    double k[] = { -1.73406162e-07, 3.18103179e-04, -2.82746259e-01, 1.05481828e+02 };
    double v = k[3];
    double pwr = adc;
    for(int i = 2; i > 0; --i) {
        v += pwr * k[i];
        pwr *= adc;
    }
    v += pwr * k[0];
    return v;
}

constexpr uint8_t f2pwm8(float v)
{
    int rv = 0;
    if(not std::isnan(v)) {
        rv = (int)(255 * v / 100);
        if(rv > 255 || rv < 0)
            return 0;
    }
    return rv;
}
Reku::Reku(IMqttPublisherSPtr mqtt, const char* ttydev)
    : temperature { { SimpleSensor { "tRekuIntake", mqtt }, SimpleSensor { "tRekuExhaust", mqtt } } }
    , rpm { { SimpleSensor { "rpmIntake", mqtt }, SimpleSensor { "rpmExhaust", mqtt } } }
{

    initFd(ttydev);

    thread = std::thread([this] {
        reku::RekuTx recvFrame;
        reku::RekuRx control;
        struct pollfd pfd = { fd, POLLIN, 0 };

        while(fd != -1) {
            if(commOk) {
                if(readFrame(recvFrame, pfd)) {
                    onNewData(recvFrame);
                }

                control.ctrl = reku::markCmd;

                int pwm8 = f2pwm8(FlowPercent);
                int pwm8Exht = pwm8;

                if(not std::isnan(FlowExhaustPercent)) {
                    pwm8Exht = f2pwm8(FlowPercent);
                }

                control.pwm[reku::INTK] = pwm8;
                control.pwm[reku::EXHT] = pwm8Exht;

                if(bypass)
                    control.ctrl |= reku::ctrl_bypass;

                control.crc = ow::calc_crc((uint8_t*)&control, sizeof(control) - 1);
                auto ok = write(control);
                if(not ok) {
                    onFailure();
                    return;
                }
            }

            const auto prevState = commOk;
            commOk = readFrame(recvFrame, pfd);
            if(commOk) {
                onNewData(recvFrame);
            } else if(prevState) {
                onFailure();
            }
        }
    });
}

Reku::~Reku()
{
    auto tmpfd = fd;
    fd = -1;
    ::close(tmpfd);
    thread.join();
}

void Reku::initFd(const char* ttydev)
{
#ifndef BYTD_SIMULATOR
    fd = ::open(ttydev, O_RDWR | O_NOCTTY);
    if(fd == -1) {
        LogSYSDIE("reku tty open");
    }
    if(not ::isatty(fd)) {
        LogDIE("not isatty {}", ttydev);
    }

    struct termios config;
    if(::tcgetattr(fd, &config) < 0) {
        LogDIE("tcgetattr {}", ttydev);
    }

    config.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
    config.c_oflag = 0;
    config.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
    config.c_cflag &= ~(CSIZE | PARENB);
    config.c_cflag |= CS8;
    config.c_cc[VMIN] = sizeof(reku::RekuTx);
    config.c_cc[VTIME] = 4;

    if(::cfsetispeed(&config, B9600) < 0 || ::cfsetospeed(&config, B9600) < 0) {
        LogDIE("set speed {}", ttydev);
    }

    if(::tcsetattr(fd, TCSAFLUSH, &config) < 0) {
        LogDIE("tcsetattr {}", ttydev);
    }

    if(::ioctl(fd, TIOCEXCL, NULL) < 0) {
        LogSYSDIE("reku tty set TIOCEXCL");
    }
#endif
    LogINFO("reku configured uart {}", ttydev);
}

static float round5(float v)
{
    return std::roundf(v / 5) * 5;
}

bool Reku::readFrame(reku::RekuTx& recvFrame, pollfd& pfd)
{
#ifndef BYTD_SIMULATOR
    constexpr unsigned tot_timeout_ms = 4 * 1000;
    constexpr unsigned timeout_ms = 500;
    constexpr auto tot_timeout_nr = tot_timeout_ms / timeout_ms;
    bool rv = false;
    auto poll_ret = ::poll(&pfd, 1, commOk ? 1000 : timeout_ms);
    if(poll_ret < 0) {
        LogSYSERR("reku poll");
    } else if(poll_ret == 0) {
        if(++timeout_cnt == tot_timeout_nr) {
            LogERR("reku comm. timeout");
        }
        return false; // expected regular timeout
    } else if(pfd.revents != POLLIN) {
        LogERR("reku poll revents {}", pfd.revents);
    } else {
        auto len = ::read(fd, &recvFrame, sizeof(recvFrame));
        if(len == sizeof(recvFrame) && ((recvFrame.stat & reku::mark_mask) == reku::markCnf || (recvFrame.stat & reku::mark_mask) == reku::markInd) && ow::check_crc((uint8_t*)&recvFrame, sizeof(recvFrame) - 1, recvFrame.crc)) {
            rv = true;
            timeout_cnt = 0;
            if(not commOk) {
                LogINFO("Reku connection");
            }
        } else {
            if(commOk) {
                LogERR("Reku connection {} {:02X} {}", len, recvFrame.stat,
                    ow::check_crc((uint8_t*)&recvFrame, sizeof(recvFrame) - 1, recvFrame.crc));
            }
        }
    }
    return rv;
#else
    std::ignore = recvFrame;
    std::ignore = pfd;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return false;
#endif
}

struct PVs {
    std::array<float, 2> rpm;
    std::array<float, 2> temp;
    bool bypass;
};

void Reku::onNewData(const reku::RekuTx& recvFrame)
{
    PVs pvs;
    pvs.bypass = bypass;
    for(unsigned i = 0; i < pvs.rpm.size(); ++i) {
        constexpr double dt = 256 * 4 / reku::crystalFoscHz;
        pvs.rpm[i] = 0;
        if(recvFrame.ch[i].period > 0)
            pvs.rpm[i] = 60 / (recvFrame.ch[i].period * dt);
        pvs.temp[i] = adc_temp(recvFrame.ch[i].temp & 0x03FF);
    }

    LogDBG("bypass:{} INTAKE: {:.1f}deg {}rpm EXHAUST: {:.1f}deg {}rpm "
           "Flow:{:.1f}%",
        pvs.bypass, pvs.temp[reku::INTK], (int)pvs.rpm[reku::INTK], pvs.temp[reku::EXHT],
        (int)pvs.rpm[reku::EXHT], FlowPercent);
    for(int i = reku::INTK; i < reku::_LAST; ++i) {
        temperature[i].setValue(pvs.temp[i]);
        rpm[i].setValue(round5(pvs.rpm[i]));
    }
}

bool Reku::write(const reku::RekuRx& frame)
{
#ifndef BYTD_SIMULATOR
    if(sizeof(frame) != ::write(fd, &frame, sizeof(frame))) {
        LogSYSERR("Reku write");
        return false;
    }
#else
    std::ignore = frame;
#endif
    return true;
}

void Reku::onFailure()
{
    for(int i = reku::INTK; i < reku::_LAST; ++i) {
        temperature[i].setValue(std::numeric_limits<float>::quiet_NaN());
        rpm[i].setValue(std::numeric_limits<float>::quiet_NaN());
    }
}
