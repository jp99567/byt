#include "OnOffDevice.h"
#include "IIo.h"


OnOffDevice::OnOffDevice(std::unique_ptr<IDigiOut> out):out(std::move(out)){}

void OnOffDevice::toggle()
{
    auto newVal = ! *out;
    (*out)(newVal);
}

void OnOffDevice::set(bool on)
{
    (*out)(on);
}
