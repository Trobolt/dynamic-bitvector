
#include<bits/stdc++.h>
#include "dynamicbitvector.h"
#include "../testutils.h"
#include "../myassert.h"

const int ROTATIONS = 1'000'000;

template<class T>
void opaque(T &&t) {
    asm volatile("" : "+r" (t));
}


void generateRots(const vector<tuple<uint64_t, uint64_t, uint64_t >> &rots, vector<uint64_t> &sizes) {
    sizes.clear();
//    sizes.reserve(1000);
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

template<WordSizes wordSize, bool useBinSearch>
void staticTest(const vector<uint64_t> &sizes, const vector<uint64_t> &ones, const vector<vector<bool>> &bools) {
    cout << wordSize << endl;
    for(int count = 0; count < sizes.size(); ++count) {
        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        BitVectorBuilder<wordSize, useBinSearch> builder(sizes[count]);
        for(int c = 0; c < sizes[count]; ++c) {
            builder.push_back(bools[count][c]);
        }
        DynamicBitVector<wordSize, useBinSearch> bv(builder);
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        cout << "-init Strategy: " << wordSize << "; Used Bits:" << (bv.sizeInBytes() * 8) << "; Input Bits: " << sizes[count] << "; Bits per Bit: " << (sc<double>(bv.sizeInBytes() * 8) / std::max(scu64(sizes[count]), scu64(1))) << "; time spend: " << (std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()) << "[ms];  time per Bit: " << (std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / sizes[count]) << "[ns]" << endl;

        begin = std::chrono::steady_clock::now();
        for(int c = 0; c < ROTATIONS; ++c) {
            const int idx = ((rand() << 16) + rand()) % sizes[count];
            bool b = bv.access(idx);
            opaque(b);
            // ALWAYS_ASSERT_E(scu32(b), scu32(bools[i][c]))
        }
        end = std::chrono::steady_clock::now();
        cout << "-access Strategy: " << wordSize << "; Used Bits:" << (bv.sizeInBytes() * 8) << "; Input Bits: " << sizes[count] << "; Bits per Bit: " << (sc<double>(bv.sizeInBytes() * 8) / std::max(scu64(sizes[count]), scu64(1))) << "; time spend: " << (std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()) << "[ms];  time per Bit: " << (std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / ROTATIONS) << "[ns]" << endl;
        begin = std::chrono::steady_clock::now();
        for(int c = 0; c < ROTATIONS; ++c) {
            const int idx = ((rand() << 16) + rand()) % sizes[count];
            uint64_t b = bv.template rank<true>(idx);
            opaque(b);
            // ALWAYS_ASSERT_E(scu32(b), scu32(bools[i][c]))
        }
        end = std::chrono::steady_clock::now();
        cout << "-rank Strategy: " << wordSize << "; Used Bits:" << (bv.sizeInBytes() * 8) << "; Input Bits: " << sizes[count] << "; Bits per Bit: " << (sc<double>(bv.sizeInBytes() * 8) / std::max(scu64(sizes[count]), scu64(1))) << "; time spend: " << (std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()) << "[ms];  time per Bit: " << (std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / ROTATIONS) << "[ns]" << endl;
        begin = std::chrono::steady_clock::now();
        for(int c = 0; c < ROTATIONS; ++c) {
            const int idx = ((rand() << 16) + rand()) % ones[count];
            uint64_t b = bv.template select<true>(idx);
            opaque(b);
            // ALWAYS_ASSERT_E(scu32(b), scu32(bools[i][c]))
        }
        end = std::chrono::steady_clock::now();
        cout << "-select Strategy: " << wordSize << "; Used Bits:" << (bv.sizeInBytes() * 8) << "; Input Bits: " << sizes[count] << "; Bits per Bit: " << (sc<double>(bv.sizeInBytes() * 8) / std::max(scu64(sizes[count]), scu64(1))) << "; time spend: " << (std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()) << "[ms];  time per Bit: " << (std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / ROTATIONS) << "[ns]" << endl;
        begin = std::chrono::steady_clock::now();
        for(int c = 0; c < ROTATIONS; ++c) {
            const int idx = ((rand() << 16) + rand()) % sizes[count];
            bv.flip(idx);
            // ALWAYS_ASSERT_E(scu32(b), scu32(bools[i][c]))
        }
        end = std::chrono::steady_clock::now();
        cout << "-flip Strategy: " << wordSize << "; Used Bits:" << (bv.sizeInBytes() * 8) << "; Input Bits: " << sizes[count] << "; Bits per Bit: " << (sc<double>(bv.sizeInBytes() * 8) / std::max(scu64(sizes[count]), scu64(1))) << "; time spend: " << (std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()) << "[ms];  time per Bit: " << (std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / ROTATIONS) << "[ns]" << endl << endl;
        //   begin = std::chrono::steady_clock::now();
        //   for(int c = 0; c < ROTATIONS; ++c) {
        //       const int idx = ((rand() << 16) + rand()) % sizes[count];
        //       bv.template insert<true>(idx);
        //       // ALWAYS_ASSERT_E(scu32(b), scu32(bools[i][c]))
        //   }
        //   end = std::chrono::steady_clock::now();
        //   cout << "-insert Strategy: " << wordSize << "; Used Bits:" << (bv.sizeInBytes() * 8) << "; Input Bits: " << sizes[count] << "; Bits per Bit: " << (sc<double>(bv.sizeInBytes() * 8) / std::max(scu64(sizes[count]), scu64(1))) << "; time spend: " << (std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()) << "[ms];  time per Bit: " << (std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / ROTATIONS) << "[ns]" << endl << endl;
    }
}

int main() {
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    const vector<tuple<uint64_t, uint64_t, uint64_t >> maxrot = {{1'000, 1'000, 1},
            //{10'000,        10'000,        1},
                                                                 {100'000, 100'000, 1},
            //{1'000'000, 1'000'000, 1},
                                                                 {10'000'000, 10'000'000, 1},
            //{100'000'000, 100'000'000, 1},
                                                                 {1'000'000'000, 1'000'000'000, 1}
    };
    vector<uint64_t> sizes, ones;
    vector<vector<bool>> bools;
    generateRots(maxrot, sizes);
    generateBools(sizes, ones, bools);
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    cout << "Initialisation took " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "ms" << endl;
    staticTest<WORDSIZE8BIT, true>(sizes, ones, bools);
    staticTest<WORDSIZE16BIT, true>(sizes, ones, bools);
    staticTest<WORDSIZE32BIT, true>(sizes, ones, bools);
    staticTest<WORDSIZE64BIT, true>(sizes, ones, bools);
}