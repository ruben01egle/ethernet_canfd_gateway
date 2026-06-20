#ifndef CRINGBUFFER_HPP
#define CRINGBUFFER_HPP

#include <stdint.h>
#include <atomic>

template<typename DATA, uint32_t SIZE>
class CRingBuffer
{
    static_assert(SIZE > 0, "Size musst be greater than 0");
public:
    CRingBuffer();
    bool push(const DATA& pData);
    bool peek(DATA& pData);
    bool pop(DATA& pData);
    uint32_t getSize();
    void reset();

private:
    static constexpr uint32_t INTERNAL_SIZE = SIZE + 1;
    DATA mData[INTERNAL_SIZE];
    std::atomic<uint32_t> mReadIdx;
    std::atomic<uint32_t> mWriteIdx;
};

#include "CRingBuffer.tpp"

#endif
