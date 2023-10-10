#include "Reku.h"
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#ifdef REKU_TESTAPP
#include <fstream>
#endif
#include "Log.h"
#include "data.h"
#include "rekuperacia_fw/reku_control_proto.h"

double adc_temp(uint16_t adc)
{
    double k[] = {-1.73406162e-07,  3.18103179e-04, -2.82746259e-01, 1.05481828e+02};
    double v = k[3];
    double pwr = adc;
    for(int i = 2; i > 0; --i){
        v += pwr * k[i];
        pwr *= adc;
    }
    v += pwr * k[0];
    return v;
}

Reku::Reku(const char* ttydev)
{
    fd = ::open(ttydev, O_RDWR | O_NOCTTY);
    if( fd == -1){
        LogSYSDIE("reku tty open");
    }
    if(not ::isatty(fd)){
        LogDIE("not isatty {}", ttydev);
    }

    struct termios  config;
    if(::tcgetattr(fd, &config) < 0){
        LogDIE("tcgetattr {}", ttydev);
    }

    config.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
    config.c_oflag = 0;
    config.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
    config.c_cflag &= ~(CSIZE | PARENB);
    config.c_cflag |= CS8;
    config.c_cc[VMIN]  = sizeof(reku::RekuTx);
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

    LogINFO("reku configured uart {}", ttydev);
    thread = std::thread([this]{
        reku::RekuTx info;
        reku::RekuRx control;
        while(fd != -1){
          if (commOk) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            control.ctrl = reku::markCmd;
            int pwm8 = (int)(255 * FlowPercent / 100);
            if (pwm8 > 255 || pwm8 < 0)
                pwm8 = 0;
            control.pwm[reku::INTK] = pwm8;
            control.pwm[reku::EXHT] = pwm8;

/*            if(pvs.temp[reku::INTK] < ByPassTemp-2)
                bypass = true;
            if(pvs.temp[reku::INTK] > ByPassTemp)
                bypass = false;
*/
            if(bypass)
                control.ctrl |= reku::ctrl_bypass;

            control.crc =
                ow::calc_crc((uint8_t *)&control, sizeof(control) - 1);

            if (sizeof(control) != ::write(fd, &control, sizeof(control))) {
              if (fd == -1)
                return;
              LogSYSDIE("Reku write");
            }
          }

            auto len = ::read(fd, &info, sizeof(info));
            if(len == sizeof(info)
                && ((info.stat&reku::mark_mask) == reku::markCnf || (info.stat&reku::mark_mask) == reku::markInd)
                && ow::check_crc((uint8_t*)&info, sizeof(info)-1, info.crc)){
                if(not commOk){
                    LogINFO("Reku connection");
                    commOk = true;
                }
                PVs tmp;
                tmp.bypass = bypass;
                for(unsigned i=0; i<pvs.rpm.size(); ++i){
                    constexpr double dt = 256 * 4 / reku::crystalFoscHz;
                    tmp.rpm[i] = 0;
                    if(info.ch[i].period > 0)
                tmp.rpm[i] = 60 / (info.ch[i].period * dt);
                    tmp.temp[i] = adc_temp(info.ch[i].temp & 0x03FF);
                }
                pvs = tmp;
            }
            else{
                if(commOk){
                    LogERR("Reku connection {} {:02X} {}", len, info.stat, ow::check_crc((uint8_t*)&info, sizeof(info)-1, info.crc));
                    commOk = false;
                }
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

#ifdef REKU_TESTAPP
int main(int argc, char* argv[])
{
    Reku reku(argv[1]);

    while(true){
        std::this_thread::sleep_for(std::chrono::seconds(2));
        LogINFO("bypass:{} INTAKE: {:.1f}deg {}rpm EXHAUST: {:.1f}deg {}rpm", reku.getPV().bypass,
                reku.getPV().temp[reku::INTK],
                (int)reku.getPV().rpm[reku::INTK],
                reku.getPV().temp[reku::EXHT],
                (int)reku.getPV().rpm[reku::EXHT]);
        std::ifstream f("reku.txt");
        int val = 0;
        f >> reku.FlowPercent;
        f >> val;
        reku.bypass = (bool)val;
    }
}
#endif
