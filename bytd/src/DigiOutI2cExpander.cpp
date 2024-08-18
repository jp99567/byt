#include "DigiOutI2cExpander.h"
#include <stdexcept>

DigiOutI2cExpander::DigiOutI2cExpander(slowswi2cpwm& ioexpander, int idx)
    : ioexpander(ioexpander)
{
    if(idx == 1)
        pDigOut = &slowswi2cpwm::dig1Out;
    else if(idx == 2)
        pDigOut = &slowswi2cpwm::dig2Out;
    else
        throw std::runtime_error("ioexpander invalid index");
}

DigiOutI2cExpander::operator bool() const
{
    return cachedVal;
}

bool DigiOutI2cExpander::operator()(bool value)
{
    (ioexpander.*pDigOut)(value);
    return cachedVal = value;
}
