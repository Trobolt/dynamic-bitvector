#ifndef ADS_DYNAMICBITVECTOR_H
#define ADS_DYNAMICBITVECTOR_H

#include<bits/stdc++.h>
#include "../bitutils.h"

using namespace std;

class slowDynBitVec {
    vector<uint64_t> vec;
    size_t _size;
public:
    size_t size() { return _size; }

    slowDynBitVec(size_t size) : _size(size), vec((size / 64) + 1) {}

    template<bool B>
    void insert(uint32_t idx) {
        assert(idx <= _size);
        uint64_t cur = vec[idx / 64];
        uint64_t overflow = cur >> 63, overflow2 = 0;
        uint64_t bmask = ((1ull << (idx % 64)) - 1);
        vec[idx / 64] = cur & bmask | ((cur & (~bmask)) << 1) | (static_cast<uint64_t> (B) << (idx % 64));
        for (uint32_t i = (idx / 64) + 1; i < vec.size(); ++i) {
            overflow2 = vec[i] >> 63;
            vec[i] = vec[i] << 1 | overflow;
            swap(overflow, overflow2);
        }
        if (!(++_size % 64))
            vec.push_back(overflow);
    }

    void deleteB(uint32_t idx) {
        assert(_size >= 0);
        uint64_t overflow = 0, overflow2 = 0;
        for (int i = vec.size() - 1; i > (idx / 64); --i) {
            overflow2 = vec[i] & 1;
            vec[i] = overflow << 63 | vec[1] >> 1;
            swap(overflow, overflow2);
        }
        uint64_t bmask = ((1ull << (idx % 64)) - 1);
        vec[idx / 64] = (overflow << 63) | (vec[idx / 64] & bmask) | (((~bmask) << 1) & vec[idx / 64]) >> 1;
        if (!(--_size % 64)) {
            vec.pop_back();
        }
    }

    bool getBit(uint32_t idx) {
        return ithsBit(vec[idx / 64], idx % 64);
    }

    template<bool B>
    uint32_t rank(uint32_t idx) {
        uint32_t idx1 = idx / 64, idx2 = idx % 64, out = 0;
        for (int i = 0; i < idx1; ++i) {
            if constexpr(B) {
                out += __builtin_popcountll(vec[i]);
            } else {
                out += __builtin_popcountll(~vec[i]);
            }
        }
        if constexpr(B) {
            out += __builtin_popcountll(vec[idx] & ((1 << idx2) - 1));
        } else {
            out += __builtin_popcountll((~(vec[idx])) & ((1 << idx2) - 1));
        }
        return out;
    }

    template<bool B>
    uint32_t select(uint32_t idx) {
        uint32_t out, temp, i;
        for (i = 0, out = 0; i < vec.size() && idx >= (temp = __builtin_popcountll(
                (B) ? vec[i] : ~vec[i])); ++i, out += 64, idx -= temp) {
        }
        if (i == vec.size())return -1;
        assert(idx < 64);
        for (int j = 0; idx > 0; ++j, ++out) {
            if constexpr(B) {
                idx -= (vec[i] >> j) & 1ull;
            } else {
                idx -= ((~vec[i]) >> j) & 1ull;
            }
        }
        return out;
    }
};

#endif //ADS_DYNAMICBITVECTOR_H
