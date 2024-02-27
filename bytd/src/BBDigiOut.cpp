#include "BBDigiOut.h"
#include "Log.h"

BBDigiOut::BBDigiOut(gpiod::chip& chip, unsigned int lineNr)
    : line(chip.get_line(lineNr))
{
    gpiod::line_request req;
    req.request_type = gpiod::line_request::DIRECTION_OUTPUT;
    req.consumer = "bytd";
    line.request(req);
    if(line.direction() != gpiod::line::DIRECTION_OUTPUT) {
        LogERR("BBDigiOut direction not output");
    }
}

BBDigiOut::operator bool() const
{
    return line.get_value();
}

bool BBDigiOut::operator()(bool value)
{
    line.set_value(value);
    return value;
}
