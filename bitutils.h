#ifndef ADS_BITUTILS_H
#define ADS_BITUTILS_H

#include "myassert.h"

using namespace std;

constexpr uint64_t reverseBits(uint64_t num) {
    num = __builtin_bswap64(num);
    num = ((num & 0xf0f0f0f0f0f0f0f0) >> 4) | ((num & 0xf0f0f0f0f0f0f0f) << 4);
    num = ((num & 0xcccccccccccccccc) >> 2) | ((num & 0x3333333333333333) << 2);
    return ((num & 0xaaaaaaaaaaaaaaaa) >> 1) | ((num & 0x5555555555555555) << 1);
}

constexpr uint64_t ithsBit(uint64_t num, uint8_t i) {
    return (num >> (i % 64)) & 1ULL;
}

uint64_t onlyOnes(int ones) {
    ASSERT_R(ones, 0, 65)
    return ones ? ((2ull << (ones - 1)) - 1) : 0;
}


void test() {
    for (int i = 0; i < 64; ++i) {
        ASSERT_E(reverseBits(1ull << i), (static_cast<uint64_t>(INT64_MIN ) >> i));
    }
    for (int i = 0; i < 64; ++i) {
        ASSERT_B(ithsBit(1ull << i, i));
    }
    for (int i = 0; i < 65; ++i) {
        ASSERT_E(__builtin_popcountll(onlyOnes(i)), i, onlyOnes(i), i)
    }
}

#endif //ADS_BITUTILS_H
