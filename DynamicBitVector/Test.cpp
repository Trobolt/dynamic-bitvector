#ifndef DYNBITVECTEST_CPP
#define DYNBITVECTEST_CPP


#include<bits/stdc++.h>
#include "dynamicbitvector.h"
#include "../testutils.h"
#include "../myassert.h"

#ifdef endl
#undef endl
#endif


void generateRots(const vector<tuple<uint64_t, uint64_t, uint64_t >> &rots, vector<uint64_t> &sizes) {
    sizes.clear();
    sizes.reserve(1000);
    for(auto x: rots) {
        uint64_t minSize, maxSize, rotations;
        std::tie(minSize, maxSize, rotations) = x;
        for(int i = 0; i < rotations; ++i) {
            sizes.emplace_back(rand() % (maxSize - minSize + 1) + minSize);
        }
    }
    sort(sizes.begin(), sizes.end());
}

void generateBools(const vector<uint64_t> &sizes, vector<uint64_t> &ones, vector<vector<bool>> &out) {
    out.clear();
    out.reserve(sizes.size());
    ones.clear();
    ones.reserve(sizes.size());
    for(int i = 0; i < sizes.size(); ++i) {
        uint64_t _ones = 0;
        vector<bool> cur(sizes[i]);
        for(int j = 0; j < sizes[i]; ++j) {
            const bool b = static_cast<bool>(rand() % 2);
            cur[j] = b;
            _ones += b;
        }
        out.emplace_back(std::move(cur));
        ones.emplace_back(_ones);
    }
}

template<WordSizes wordsize, bool useBinSearch>
void initTest(const vector<uint64_t> &sizes) {
    for(int i = 0; i < sizes.size(); ++i) {
        // if (i == 0) cout << FILELINE << endl;
        BitVectorBuilder<wordsize, true> builder(sizes[i]);
    }
}

template<WordSizes wordSize, bool useBinSearch>
uint64_t staticTest(const vector<uint64_t> &sizes, const vector<uint64_t> &ones, const vector<vector<bool>> &bools) {
    ASSERT_E(std::accumulate(sizes.begin(), sizes.end(), 0ull),
             std::accumulate(bools.begin(), bools.end(), 0ull,
                             [](const uint64_t &b1, const vector<bool> &b2) { return b1 + b2.size(); }))
    ASSERT_E(sizes.size(), bools.size())
    for(int i = 0; i < sizes.size(); ++i) {
        BitVectorBuilder<wordSize, true> builder(sizes[i]);
        for(int c = 0; c < sizes[i]; ++c) {
            builder.push_back(bools[i][c]);
        }
        builder.clearAndCheck();
        for(int c = 0; c < sizes[i]; ++c) {
            const bool b = builder.next();
            ALWAYS_ASSERT_E(b, bools[i][c])
        }
        DynamicBitVector<wordSize, true> bv(builder);
        ALWAYS_ASSERT_E(bv.ones(), ones[i])
        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        //  cout << "-flip- Strategy: " << (useBinSearch ? " binary " : " sequential") << "; Used Bits:" << (bv.sizeInBytes() * 8) << "; Input Bits: " << sizes[i] << "; Bits per Bit: " << (sc<double>(bv.sizeInBytes() * 8) / std::max(scu64(sizes[i]), scu64(1))) << "; time spend: " << (std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()) << "[ms];  time per Bit: " << (std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / std::max(sizes[i], scu64(1))) << "[ns]" << endl;

        begin = std::chrono::steady_clock::now();
        ASSERT_CODE_GLOBALE(uint64_t assert_ones = bv.ones();)
        for(int64_t c = 0; c < sizes[i]; ++c) {
            const bool b = bv.access(c);
            ALWAYS_ASSERT_E(b, bools[i][c])
            uint64_t numOnes = bv.template rank<true>(c);
            if(numOnes != 0) {
                uint64_t lastOne = bv.template select<true>(numOnes - 1);
                ALWAYS_ASSERT_E(bv.access(lastOne), true)
                ALWAYS_ASSERT_E(1 + lastOne + bv.template rank<false>(c) - bv.template rank<false>(lastOne), c)
            } else {
                ALWAYS_ASSERT_E(bv.template rank<false>(c), c)
            }
        }
        for(int c = 0; c < sizes[i]; ++c) {
            bv.flip(c);
        }
        ASSERT_E(bv.ones(), bv.nums() - assert_ones)
        for(int64_t c = 0; c < sizes[i]; ++c) {
            const bool b = bv.access(c);
            ALWAYS_ASSERT_E(scu32(b), scu32(!bools[i][c]))
            uint64_t numOnes = bv.template rank<false>(c);
            if(numOnes != 0) {
                uint64_t lastOne = bv.template select<false>(numOnes - 1);
                ALWAYS_ASSERT_E(bv.access(lastOne), false)
                ALWAYS_ASSERT_E(1 + lastOne + bv.template rank<true>(c) - bv.template rank<true>(lastOne), c)
            } else {
                ALWAYS_ASSERT_E(bv.template rank<true>(c), c)
            }
        }
        end = std::chrono::steady_clock::now();
        cout << i << "/" << (sizes.size() - 1) << "-access- Strategy: " << (useBinSearch ? "binary" : "sequential") << "; Used Bits:" << (bv.sizeInBytes() * 8) << "; Input Bits: " << sizes[i] << "; Bits per Bit: " << (sc<double>(bv.sizeInBytes() * 8) / std::max(scu64(sizes[i]), scu64(1))) << "; time spend: " << (std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()) << "[ms];  time per Bit: " << (std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / std::max(sizes[i], scu64(1))) << "[ns]" << endl;
    }
    return 0;
}

template<WordSizes wordSize, bool useBinSearch>
void insertDeleteTest(const vector<uint64_t> &sizes, const vector<uint64_t> &ones, const vector<vector<bool>> &bools) {
    ASSERT_E(std::accumulate(sizes.begin(), sizes.end(), 0ull),
             std::accumulate(bools.begin(), bools.end(), 0ull,
                             [](const uint64_t &b1, const vector<bool> &b2) { return b1 + b2.size(); }))
    ASSERT_E(sizes.size(), bools.size())
    {
        DynamicBitVector<wordSize, useBinSearch> bv;
        bv.template insert<true>(0);
        for(uint64_t i = 1; false; i++) {
            uint64_t key = ((rand() << 16) + rand());
            bool b = (rand() % 2);
            if(b) {
                bv.template insert<true>(key % i);
            } else {
                bv.template insert<true>(key % i);
            }
        }
    }
    for(uint64_t i = 0; i < sizes.size(); ++i) {
        DynamicBitVector<wordSize, useBinSearch> bv1;
        DynamicBitVector<wordSize, useBinSearch> bv2;
        DynamicBitVector<wordSize, useBinSearch> bv;
        for(uint64_t j = 0; j < sizes[i]; ++j) {
            //   if(i == 41 && j == 18432)                cout << FILELINE << endl;
            if(bools[i][j]) {
                bv1.template insert<true>(scu16(0));
                bv2.template insert<true>(j);
            } else {
                bv1.template insert<false>(scu16(0));
                bv2.template insert<false>(j);
            }
            ALWAYS_ASSERT_R(bv1.template rank<true>(bv1.nums() - 1), std::min(scu64(0), bv1.ones() - 1), bv1.ones() + 1)
            ALWAYS_ASSERT_R(bv2.template rank<true>(bv2.nums() - 1), std::min(scu64(0), bv2.ones() - 1), bv2.ones() + 1)
            if(i == 0) {
                for(int k = 0; k < j; ++k) {
                    //  if(k == 578)                        cout << FILELINE << endl;
                    ALWAYS_ASSERT_E(bv1.access(k), bools[i][j - k])
                    ALWAYS_ASSERT_E(bv2.access(k), bools[i][k])
                }
            }
        }
        uint64_t prevPow = (sizes[i] != 0) ? scu64(1) << (64 - __builtin_clzll(sizes[i]) - (__builtin_popcountll(sizes[i]) == 1)) : 0;
        for(uint64_t curPow = prevPow >> 1; curPow > 0; prevPow >>= 1, curPow >>= 1) {
            for(uint64_t key = curPow, accKey = 0; key < sizes[i]; key += prevPow, accKey += 2) {
                // if(i == 42 && curPow == 2 && key >= 67394) {                  cout << FILELINE << endl;              }
                if(bools[i][key])
                    bv.template insert<true>(accKey);
                else
                    bv.template insert<false>(accKey);
                ALWAYS_ASSERT_E(scu32(bv.access(accKey)), bools[i][key])
                ALWAYS_ASSERT_R(bv.template rank<true>(bv.nums() - 1), std::min(scu64(0), bv.ones() - 1), bv.ones() + 1)
                if(i == 0 && curPow < 3 && key >= 67390) {
                    for(uint64_t x = curPow, y = 0; x <= key; x += curPow, ++y) {
                        if(i == 42 && y == 25726)
                            cout << FILELINE << endl;
                        ALWAYS_ASSERT_E(scu32(bv.access(y)), bools[i][x])
                    }
                }
            }
        }
        if(sizes[i] != 0) {
            if(bools[i][0]) bv.template insert<true>(0);
            else bv.template insert<false>(0);
        }
        for(int j = 0; j < sizes[i]; ++j) {
            ALWAYS_ASSERT_E(scu32(bv.access(j)), bools[i][j])
            ALWAYS_ASSERT_E(scu32(bv1.access(j)), bools[i][sizes[i] - j - 1])
            ALWAYS_ASSERT_E(scu32(bv2.access(j)), bools[i][j])
        }
        prevPow = (sizes[i] != 0) ? scu64(1) << (64 - __builtin_clzll(sizes[i]) - (__builtin_popcountll(sizes[i]) == 1)) : 0;

    }

}

template<WordSizes wordSize, bool useBinarySearch>
void test(const vector<uint64_t> &sizes, const vector<uint64_t> &ones, const vector<vector<bool>> &bools) {
    cout << "-----Test for wordSize = " << wordSize << endl;
    std::chrono::steady_clock::time_point begin, end;
#warning "Node<0, wordSize, useBinarySearch>::test(); is off"
    // NODE0::test();
    NODE2::test();
    NODE3::test();
    NODE4::test();
    begin = std::chrono::steady_clock::now();
    initTest<wordSize, useBinarySearch>(sizes);
    end = std::chrono::steady_clock::now();
    std::cout << "initTest runs in: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()
              << "[µs] for " << std::accumulate(bools.begin(), bools.end(), 0ull,
                                                [](const uint64_t &b1, const vector<bool> &b2) {
                                                    return b1 + b2.size();
                                                }) << " Bits" << std::endl;
    insertDeleteTest<wordSize, useBinarySearch>(sizes, ones, bools);
    begin = std::chrono::steady_clock::now();
    const uint64_t size = staticTest<wordSize, useBinarySearch>(sizes, ones, bools);
    end = std::chrono::steady_clock::now();
    std::cout << "staticTest runs in: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()
              << "[µs] for " << std::accumulate(bools.begin(), bools.end(), 0ull,
                                                [](const uint64_t &b1, const vector<bool> &b2) {
                                                    return b1 + b2.size();
                                                }) << " Bits and used " << size << " Bytes." << std::endl;
}


void quicktest() {
    union {
        uint16_t shorts[4];
        uint64_t longs;
    } un;
    un.longs = 0x0123'4567'89ab'cdef;
    ALWAYS_ASSERT_E(un.shorts[0], 0xcdef, "The compiler or the endiannes are wrong...")
    ALWAYS_ASSERT_E(un.shorts[1], 0x89ab, "The compiler or the endiannes are wrong...")
    ALWAYS_ASSERT_E(un.shorts[2], 0x4567, "The compiler or the endiannes are wrong...")
    ALWAYS_ASSERT_E(un.shorts[3], 0x0123, "The compiler or the endiannes are wrong...")
    uint64_t longs = 0x0123'4567'89ab'cdef;
    uint16_t shorts[4];
    std::memcpy(&shorts, &longs, sizeof(longs));
    ALWAYS_ASSERT_E(shorts[0], 0xcdef, "The compiler or the endiannes are wrong...")
    ALWAYS_ASSERT_E(shorts[1], 0x89ab, "The compiler or the endiannes are wrong...")
    ALWAYS_ASSERT_E(shorts[2], 0x4567, "The compiler or the endiannes are wrong...")
    ALWAYS_ASSERT_E(shorts[3], 0x0123, "The compiler or the endiannes are wrong...")
}

int main() {
    quicktest();
    vector<tuple<uint64_t, uint64_t, uint64_t >> maxrot = {{0,                 0,         1},
                                                           {0,                 100,       10},
                                                           {100,               1000,      10},
                                                           {1000,              10000,     20},
                                                           {100000,            1000000,   10},
                                                           {10000000 - 100000, 100000000, 1}//,                                                           {1'000'000'000 - 100000, 1'000'000'000, 1}
    };
    ASSERT_E(__builtin_clzll(0x7fff'ffff'ffff'ffff), 1)
    vector<uint64_t> sizes, ones;
    vector<vector<bool>> bools;
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    generateRots(maxrot, sizes);
    generateBools(sizes, ones, bools);
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    cout << "-Initialisation took " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "ms" << endl;
    test<WORDSIZE8BIT, true>(sizes, ones, bools);
    test<WORDSIZE32BIT, true>(sizes, ones, bools);
    test<WORDSIZE16BIT, true>(sizes, ones, bools);
    test<WORDSIZE64BIT, true>(sizes, ones, bools);
    return 0;
}

#define endl '\n'
#endif