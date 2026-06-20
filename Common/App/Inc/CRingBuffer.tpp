#include "CRingBuffer.hpp"

template<typename DATA, uint32_t SIZE>
CRingBuffer<DATA, SIZE>::CRingBuffer(): mReadIdx(0), mWriteIdx(0)
{
}

template<typename DATA, uint32_t SIZE>
bool CRingBuffer<DATA, SIZE>::push(const DATA& pData)
{
    uint32_t rIdx = mReadIdx.load();
    uint32_t wIdx = mWriteIdx.load();
    uint32_t nextWIdx = (wIdx + 1) % INTERNAL_SIZE;
    if (nextWIdx == rIdx) {
        return false;
    }
    mData[wIdx] = pData;
    mWriteIdx.store(nextWIdx);
    return true;
}

template <typename DATA, uint32_t SIZE>
inline bool CRingBuffer<DATA, SIZE>::peek(DATA &pData)
{
    uint32_t rIdx = mReadIdx.load();
    uint32_t wIdx = mWriteIdx.load();
    if (rIdx == wIdx) {
        return false;
    }
    pData = mData[rIdx];
    return true;
}

template<typename DATA, uint32_t SIZE>
bool CRingBuffer<DATA, SIZE>::pop(DATA& pData)
{
    uint32_t rIdx = mReadIdx.load();
    uint32_t wIdx = mWriteIdx.load();
    if (rIdx == wIdx) {
        return false;
    }
    uint32_t nextRIdx = (rIdx + 1) % INTERNAL_SIZE;
    pData = mData[rIdx];
    mReadIdx.store(nextRIdx);
    return true;
}

template<typename DATA, uint32_t SIZE>
uint32_t CRingBuffer<DATA, SIZE>::getSize()
{
    uint32_t rIdx = mReadIdx.load();
    uint32_t wIdx = mWriteIdx.load();

    return ((INTERNAL_SIZE + wIdx - rIdx) % INTERNAL_SIZE);
}

template <typename DATA, uint32_t SIZE>
void CRingBuffer<DATA, SIZE>::reset()
{
    mReadIdx.store(0);
    mWriteIdx.store(0);
}
