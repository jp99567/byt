#include "BytRequest.h"

BytRequestFactory::BytRequestFactory(std::shared_ptr<ICore> core)
    :core(core){}

doma::BytRequestIf *BytRequestFactory::getHandler(const apache::thrift::TConnectionInfo &connInfo)
{
    return new BytRequest(core);
}

void BytRequestFactory::releaseHandler(doma::BytRequestIf *p)
{
    delete p;
}
