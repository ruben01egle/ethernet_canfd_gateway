#include "CInterCoreLink.hpp"

#include <new>

CInterCoreLink &CInterCoreLink::getInstance()
{
    #ifdef CORE_CM7
        static CInterCoreLink instance(true);
    #else
        static CInterCoreLink instance(false);
    #endif
    return instance;
}

CInterCoreLink::CInterCoreLink(bool pMasterInit): mSharedMemory(*reinterpret_cast<sharedMemory*>(SRAM4_START))
{
    if (pMasterInit) {
        new (&mSharedMemory) sharedMemory();
        reset();
        mSharedMemory.magic_byte.store(INIT_MAGIC);
    }
}

bool CInterCoreLink::isInitalized()
{
    return (mSharedMemory.magic_byte.load() == INIT_MAGIC);
}

void CInterCoreLink::reset()
{
    mSharedMemory.log_msg.reset();
    mSharedMemory.ip_addr.reset();
    mSharedMemory.avgBusTimeUs.store(0);
}
