#ifndef ADS_DYNAMICBITVECTOR_H
#define ADS_DYNAMICBITVECTOR_H

#if !(__BYTE_ORDER == __LITTLE_ENDIAN || __BYTE_ORDER == __BIG_ENDIAN)
#error "System needs to be __LITTLE_ENDIAN or __BIG_ENDIAN or the wrong compiler is used"
#endif

#include<bits/stdc++.h>
#include <immintrin.h>
#include <x86intrin.h>
#include <ltdl.h>
#include "../myassert.h"
#include "../bitutils.h"
#include "samevaluearray.h"

//TODO: BMI2 __builtin_ia32_pext_diPDEP improve

#define NODE4 Node<4, wordSize, useBinarySearch>
#define NODE3 Node<3, wordSize, useBinarySearch>
#define NODE2 Node<2, wordSize, useBinarySearch>
#define NODE0 Node<0, wordSize, useBinarySearch>
#define DYNAMIC_BV DynamicBitVector<wordSize, useBinarySearch>
#define NODE_UN(lvl) Node<lvl, wordSize, useBinarySearch>
#define RCNODE_4 rc<NODE4*>
#define RCNODE_3 rc<NODE3*>
#define RCNODE_2 rc<NODE2*>
#define RCNODE_0 rc<NODE0*>
#define RCNODE_UN(lvl) rc<NODE_UN(lvl)*>
#define SIZE_ASSERT(size) static_assert((size)==8||(size)==16||(size)==32||(size)==64,"Size in bit is not in {8,16,32,64}");

enum WordSizes {
    WORDSIZE8BIT = 8, WORDSIZE16BIT = 16, WORDSIZE32BIT = 32, WORDSIZE64BIT = 64
};

template<uint32_t curLevel, WordSizes wordSize, bool useBinSearch = true>
class Node;

template<WordSizes wordSize, bool useBinarySearch>
class DynamicBitVector;

template<WordSizes wordSize, bool useBinarySearch>
class BitVectorBuilder {
    //baut einen Baum über sequentiell geordnete Bits in linear Zeit.
    friend DynamicBitVector<wordSize, useBinarySearch>;
    struct {
        uint32_t onesSize;
        uint64_t *ones;
    } onesArray;
    SameValueArray<int64_t, uint16_t> curSizes, nextSizes;
    SameValueArray<int64_t, uint64_t> curNums;
    struct {
        NODE0 **leaves;
        uint32_t leavesSize;
    } leaveArray;
    struct {
        uint64_t *iterator = nullptr;
        uint32_t curNumIndex = 0;
        uint32_t nextNumIndex = 0;
        uint32_t childIndex = 0;
        uint32_t leaveIndex = 0;
        uint8_t offset = 0;
    } indices;

    ASSERT_CODE_GLOBALE(uint64_t assertOnes = 0;)

    constexpr uint64_t getSize(uint64_t size) {
        return (size * 4) / (3 * wordSize * wordSize);
    }


public:
    /**
     * create BitvectorBuilder with size
     * @param size the size
     */
    explicit BitVectorBuilder(const uint64_t size) {
        const uint64_t vecSize = getSize(size), restSize = ((size * 4) % (3 * wordSize * wordSize) / 4);
        curSizes.clear(vecSize);
        curNums.clear(vecSize);
        onesArray.ones = new uint64_t[(vecSize + 1)];
        onesArray.onesSize = (vecSize + 1);
        for(int i = 0; i < onesArray.onesSize; ++i) {
            onesArray.ones[i] = 0;
        }
        ASSERT_E(std::accumulate(onesArray.ones, onesArray.ones + onesArray.onesSize, 0ull), 0)
        ASSERT_L(size, (DynamicBitVector<wordSize, useBinarySearch>::getMax().second))
        ASSERT_E(((vecSize * wordSize * wordSize * 3) / 4) + restSize, size)
        curSizes.setAll((wordSize * wordSize * 3) / 4);
        curNums.setAll((wordSize * wordSize * 3) / 4);
        curSizes.template distributeEvenly<((wordSize * wordSize) / 4)>(restSize);
        curNums.template distributeEvenly<((wordSize * wordSize) / 4)>(restSize);
        onesArray.onesSize = curSizes.size();
        nextSizes.clear(curSizes.size() / ((3 * NODE2::numChildren) / 4));
        nextSizes.setAll((3 * NODE2::numChildren) / 4);
        nextSizes.template distributeEvenly<(NODE2::numChildren) / 4>(
                curSizes.size() % ((3 * NODE2::numChildren) / 4));
        leaveArray.leavesSize = nextSizes.size();
        leaveArray.leaves = (rc<NODE0 **>(malloc(sizeof(NODE0 *) * (leaveArray.leavesSize))));
        for(int i = 0; i < leaveArray.leavesSize; ++i) {
            leaveArray.leaves[i] = DynamicBitVector<wordSize, useBinarySearch>::allocateLeaves();
            ASSERT_E(rc<uintptr_t>(leaveArray.leaves[i]) % 64, 0)
        }
        ASSERT_E(curSizes.size(), std::max(scu64(1), (restSize < ((wordSize * wordSize) / 4)) ? vecSize : vecSize + 1))
        ASSERT_GE((curSizes.size() > 1) ? curSizes[curSizes.size() - 2] : UINT16_MAX, (wordSize * wordSize) / 2)
        ASSERT_E(curSizes.sum(), size)
        ASSERT_E(nextSizes.sum(), curSizes.size(), nextSizes.size(), sc64(curSizes.size() - nextSizes.sum()))
        ASSERT_PGE(vecSize != 0, curSizes[curSizes.size() - 1], (wordSize * wordSize) / 2)
        ASSERT_E(leaveArray.leavesSize, nextSizes.size())
        indices.iterator = rc<uint64_t *>(leaveArray.leaves[0]);
        *indices.iterator = 0;

    }

    ~BitVectorBuilder() {
        if(leaveArray.leaves != nullptr) {
            for(NODE0 **x = leaveArray.leaves;
                x < leaveArray.leaves + leaveArray.leavesSize; ++x) {
                ASSERT_NE(*x, nullptr)
                free(*x);
            }
            free(leaveArray.leaves);
        }
        if(onesArray.ones != nullptr) {
            delete[](onesArray.ones);
        }
    }


    /**
     * push_back a bool.
     * ensure that this function can only be called n times where n is the size
     * @param val
     */
    void push_back(bool val) {
        //IDEE:
        // füge hinten ein Bit an, und update alle indizes.
        // Es wird nur die unterste Ebene hier befüllt.
        // Alle anderen Ebenen werden später gebaut.
        // Jeder unterste Knoten wird zu 3/4 befüllt und so, dass wenn die 1. Ebene gefüllt wird diese auch zu 3/4 gefüllt wird.
        ASSERT_CODE_LOCALE(assertOnes += val;)
        ASSERT_L(indices.leaveIndex, leaveArray.leavesSize)
        ASSERT_L(indices.curNumIndex, curSizes.size())
        ASSERT_L(indices.nextNumIndex, nextSizes.size())
        ASSERT_L(indices.childIndex, nextSizes[indices.nextNumIndex])
        ASSERT_R(indices.offset, 0, 64)
        *indices.iterator |= (scu64(val) << indices.offset);//set bit
        if(curSizes[indices.curNumIndex] % 64 == 0) {//trivial case
            if(++indices.offset < 64) {//only offset must be updated
                return;
            } else {
                onesArray.ones[indices.curNumIndex] += __builtin_popcountll(*indices.iterator);//update _ones
                //update iterator then check if the current leave is more than 75% full, or curSizes[curNumIndex] bits have been set
                if(++indices.iterator >= rc<uint64_t *>(RCNODE_0(leaveArray.leaves[indices.leaveIndex]) + indices.childIndex) + (curSizes[indices.curNumIndex] / 64)) {
                    ++indices.curNumIndex;//next leave
                    if((++indices.childIndex) < nextSizes[indices.nextNumIndex]) {
                        //current node is not full, so next leave
                        indices.iterator = rc<uint64_t *>(
                                rc<NODE0 *>(leaveArray.leaves[indices.leaveIndex]) +
                                indices.childIndex );
                    } else {
                        ++indices.nextNumIndex;
                        indices.childIndex = 0;
                        //current node is full, so next leave
                        if(++indices.leaveIndex == leaveArray.leavesSize)return;
                        indices.iterator = rc<uint64_t *>(leaveArray.leaves[indices.leaveIndex]);
                    }
                }
                *indices.iterator = 0;//zero initialize current 64bit int
                indices.offset = 0;//first index
            }
            ASSERT_E(*indices.iterator, 0)
        } else {
            //same condition as above
            /*  if(onesArray._ones[indices.curNumIndex] >= 126) {
                  cout << FILELINE << endl;
                  cout.flush();
                  printAll((rc<uintptr_t>(indices.iterator) << 3) + (indices.offset), (rc<uintptr_t>(RCNODE_0(leaveArray.leaves[indices.leaveIndex]) + indices.childIndex) << 3) + (curSizes[indices.curNumIndex]),
                           ((rc<uintptr_t>(RCNODE_0(leaveArray.leaves[indices.leaveIndex]) + indices.childIndex) << 3) + (curSizes[indices.curNumIndex])) - ((rc<uintptr_t>(indices.iterator) << 3) + (indices.offset)));
              }*/
            if(val)++onesArray.ones[indices.curNumIndex];
            if((rc<uintptr_t>(indices.iterator) << 3) + (++indices.offset) >=
               (rc<uintptr_t>(RCNODE_0(leaveArray.leaves[indices.leaveIndex]) + indices.childIndex) << 3) + (curSizes[indices.curNumIndex])) {
                ++indices.curNumIndex;
                if(++indices.childIndex < nextSizes[indices.nextNumIndex]) {
                    indices.iterator = rc<uint64_t *>(
                            RCNODE_0(leaveArray.leaves[indices.leaveIndex]) + indices.childIndex);
                } else {
                    ++indices.nextNumIndex;
                    indices.childIndex = 0;
                    if(++indices.leaveIndex == leaveArray.leavesSize)return;
                    indices.iterator = rc<uint64_t *>(leaveArray.leaves[indices.leaveIndex]);
                }
                *indices.iterator = 0;
                indices.offset = 0;
            }
            if(indices.offset >= 64) {
                indices.offset = 0;
                ++indices.iterator;
                *indices.iterator = 0;
            }
        }
        ASSERT_NE(indices.iterator, 0, "too many bits")
        ASSERT_LE(onesArray.ones[indices.curNumIndex], curNums[indices.curNumIndex])
    }

    /**
     * testing only
     */
    void clearAndCheck() {
        ASSERT_CODE_LOCALE(
                uint64_t temp = 0;
                for(int i = 0; i < onesArray.onesSize; ++i) {
                    temp += onesArray.ones[i];
                    ASSERT_LE(onesArray.ones[i], curSizes[i], i)
                }
                ASSERT_E(temp, assertOnes)
        )
        indices.iterator = rc<uint64_t *>(leaveArray.leaves[0]);
        indices.offset = 0;
        indices.leaveIndex = 0;
        indices.curNumIndex = 0;
        indices.nextNumIndex = 0;
        indices.childIndex = 0;
    };

    /**
     * testing only
     */
    bool next() {
        ASSERT_L(indices.leaveIndex, leaveArray.leavesSize)
        ASSERT_L(indices.curNumIndex, curSizes.size())
        ASSERT_L(indices.nextNumIndex, nextSizes.size())
        ASSERT_L(indices.childIndex, nextSizes[indices.nextNumIndex])
        ASSERT_R(indices.offset, 0, 64)
        const bool out = (*indices.iterator >> indices.offset) & 1ull;
        if(curSizes[indices.curNumIndex] % 64 == 0) {//trivial case
            if(++indices.offset < 64) {//only offset must be updated
                return out;
            } else {
                //update iterator then check if the current leave is more than 75% full, or curSizes[curNumIndex] bits have been set
                if(++indices.iterator >= rc<uint64_t *>(
                                                 rc<NODE0 *>(leaveArray.leaves[indices.leaveIndex]) +
                                                 indices.childIndex) + (curSizes[indices.curNumIndex] / 64)) {
                    ++indices.curNumIndex;//next leave
                    if((++indices.childIndex) < nextSizes[indices.nextNumIndex]) {
                        //current node is not full, so next leave
                        indices.iterator = rc<uint64_t *>(
                                rc<NODE0 *>(leaveArray.leaves[indices.leaveIndex]) +
                                indices.childIndex );
                    } else {
                        ++indices.nextNumIndex;
                        indices.childIndex = 0;
                        //current node is full, so next leave
                        if(++indices.leaveIndex == leaveArray.leavesSize)return out;
                        indices.iterator = rc<uint64_t *>(leaveArray.leaves[indices.leaveIndex]);
                    }
                }
                indices.offset = 0;//first index
            }
        } else {
            //same condition as above
            if((rc<uintptr_t>(indices.iterator) << 3) + (++indices.offset) >=
               (rc<uintptr_t>(RCNODE_0(leaveArray.leaves[indices.leaveIndex]) + indices.childIndex) << 3) + (curSizes[indices.curNumIndex])) {
                ++indices.curNumIndex;
                if(++indices.childIndex < nextSizes[indices.nextNumIndex]) {
                    indices.iterator = rc<uint64_t *>(
                            rc<NODE0 *>(leaveArray.leaves[indices.leaveIndex]) + indices.childIndex);
                } else {
                    ++indices.nextNumIndex;
                    indices.childIndex = 0;
                    if(++indices.leaveIndex == leaveArray.leavesSize)return out;
                    indices.iterator = rc<uint64_t *>(leaveArray.leaves[indices.leaveIndex]);
                }
                indices.offset = 0;
            }
            if(indices.offset >= 64)indices.offset = 0, ++indices.iterator;
        }
        ASSERT_NE(indices.iterator, 0, "too many bits")
        return out;
    }

private:
    template<uint32_t nextLevel, uint32_t curLevel>
    bool check(const uint64_t sum, uint_fast8_t lvl, DynamicBitVector<wordSize, useBinarySearch> *tree) {
        {
            curSizes = nextSizes;
            nextSizes.clear(curSizes.size() / ((3 * NODE_UN(nextLevel)::numChildren) / 4));
            nextSizes.setAll((3 * NODE_UN(nextLevel)::numChildren) / 4);
            nextSizes.template distributeEvenly<(NODE_UN(nextLevel)::numChildren) / 4>(
                    curSizes.size() % ((3 * NODE_UN(nextLevel)::numChildren) / 4));
            ASSERT_E(nextSizes.sum(), curSizes.size(), nextSizes.size(), sc64(curSizes.size() - nextSizes.sum()))
        }
        ASSERT_E(leaveArray.leavesSize, curSizes.size())
        ASSERT_CODE_GLOBALE(int prevLeaveSize = leaveArray.leavesSize;)
        indices.leaveIndex = 0;
        uint64_t curBegin = 0;
        leaveArray.leavesSize = nextSizes.size();
        ASSERT_E(curNums.size(), curSizes.sum())
        for(int i = 0; i < leaveArray.leavesSize; ++i) {
            NODE_UN(curLevel) *nextArr = DynamicBitVector<wordSize, useBinarySearch>::template allocateNode<nextLevel, curLevel>();
            for(int j = 0; j < nextSizes[i]; ++j, curBegin += curSizes[indices.leaveIndex], ++indices.leaveIndex) {
                nextArr[j].clearMetadata();
                nextArr[j].setChildrenPtr(rcbyte(leaveArray.leaves[indices.leaveIndex]));
                ASSERT_L(nextSizes[i], (NODE_UN(curLevel)::numChildren))
                ASSERT_NE(curSizes[indices.leaveIndex], 0)
                ASSERT_L(indices.leaveIndex, onesArray.onesSize)
                onesArray.ones[indices.leaveIndex] = nextArr[j].init(curNums, onesArray.ones, curBegin, curSizes[indices.leaveIndex]);
                ASSERT_E(scu64(nextArr[j].size()), curSizes[indices.leaveIndex])
            }
            leaveArray.leaves[i] = rc<NODE0 *>(nextArr);
        }
        ASSERT_E(curBegin, curSizes.sum())
        ASSERT_E(indices.leaveIndex, prevLeaveSize)
        if(nextSizes.sumEqualsOne()) {
            ASSERT_E(curNums.size(), curSizes.sum())
            createTree(lvl, tree);
            return true;
        }  //
        {
            ASSERT_GE(curSizes.size(), 2)
            const uint64_t maxFill = curNums[0];
            curNums.clear(curSizes.size());
            curNums.setFirstN(maxFill * curSizes[0]);
            curNums.setNextToLast(maxFill * curSizes[curSizes.size() - 2]);
            curNums.setLast(sum - (curNums.sum()));
            ASSERT_E(curNums.sum(), sum, sum - curNums.sum())
            ASSERT_GE(curNums[curNums.size() - 1], maxFill * (curSizes[curSizes.size() - 1] - 1))
        }
        return false;
    }

    inline void createTree(uint_fast8_t lvl, DynamicBitVector<wordSize, useBinarySearch> *tree) {
        ASSERT_E(leaveArray.leavesSize, 1)
        ASSERT_PGR(lvl != 0, curSizes.sum(), 1)
        tree->_nums = curNums.sum();
        tree->_ones = onesArray.ones[0];
        tree->root = rcbyte(leaveArray.leaves[0]);
        tree->height = lvl;
        free(leaveArray.leaves);
        delete[](onesArray.ones);
        onesArray.ones = nullptr;
        leaveArray.leaves = nullptr;
    }

public:

    void createTree(DynamicBitVector<wordSize, useBinarySearch> *tree) {
        //Idee, gehe von der untersten Ebene nach oben,und baue so einen Baum.
        // Jeder Knoten ist zu 3/4 befüllt.
        // Der Baum ist fertig, wenn die  nächste Ebene nur noch einen Kindknoten besitzen würde.
        const uint64_t sum = curNums.sum();
        if(nextSizes.sumEqualsOne()) {
            createTree(0, tree);
            return;
        }

        uint_fast8_t lvlIdx = 1;
        for(; DynamicBitVector<wordSize, useBinarySearch>::level[lvlIdx] == 2 &&
              DynamicBitVector<wordSize, useBinarySearch>::level[lvlIdx + 1] == 2; ++lvlIdx) {
            if(check<2, 2>(sum, lvlIdx, tree))return;
        }
        if(check<3, 2>(sum, lvlIdx, tree))return;
        ++lvlIdx;
        for(; DynamicBitVector<wordSize, useBinarySearch>::level[lvlIdx] == 3 &&
              DynamicBitVector<wordSize, useBinarySearch>::level[lvlIdx + 1] == 3; ++lvlIdx) {
            if(check<3, 3>(sum, lvlIdx, tree))return;
        }
        if(check<4, 3>(sum, lvlIdx, tree))return;
        ++lvlIdx;
        for(; DynamicBitVector<wordSize, useBinarySearch>::level[lvlIdx] == 4 &&
              DynamicBitVector<wordSize, useBinarySearch>::level[lvlIdx + 1] == 4; ++lvlIdx) {
            if(check<4, 4>(sum, lvlIdx, tree))return;
        }
        ASSERT_UNREACHEABLE()
    }

};

template<WordSizes wordSize, bool useBinarySearch>
class DynamicBitVector {

    //Aufbau des Bitvektors:
    //Idee: baue einen binärbaum, indem eine Menge an Knoten in einem Realen Knoten stecken.
    //Ein Knoten ist jeweils 64 byte aligned, und besteht aus einem Pointer, und 2 Arrays der länge des Knoten Grads des Arrayindiziertem knoten, bzw wird jeder Knoten auf 64 byte aufgefüllt.
    //Metadaten wie Größe die Indizes u dem Richtigen kind werden in allen freien Bits gespeichert, so das die metadaten kostenlos sind. (bspw. sind die hintersten 6 bits des Pointers immer 0)
    //Also wie ein B Baum, nur mit Binärbäumen als Knoten.
    //In der untersten Schicht sind die jeweiligen statische Bitvektoren.
    //Dann sind so viele 16Bit schichten, bis diese Überlaufen könnten.
    //Dann 32 Bit schichten, usw...
    //Jede Schicht speichert einen Pointer, auf n kontinuierliche Blätter. (z.b. n = 15 bei 16 bit)
    //Der Baum wird analog zu einem B Baum "balanciert". (Theoretisch kriegt man so auch leicht hin, das Jeder Knoten / Blatt zu immer mind. 2/3 gefüllt sind).
    //Da ich nicht den BP baum implementieren konnte, beschreibe ich, wie man diese Datenstrucktur erweitern hätter können.
    //Excess im Intervall von [0,x] lässt sich leicht durch rank Anfragen berechnen, allerdings hätte man praktisch noch kostenlos speicherplatz (Da der nicht allociert werden kann), in dem man trotzdem excess speichern kann.
    //Also hätte ich die größe der Blätter auf 128 byte verdoppelt und in diesen alle Mins gespeichert.
    //das Balancieren ist damit genauso schwer wie vorher, da wenn ein Block verschoben wird, das minimum identisch bleibt.
    //Da im vergleich zum echten binären suchbaum der Min für jeden Knoten gespeichert werden kann, bracht man für die backwardsearch kein maximum.

    friend BitVectorBuilder<wordSize, useBinarySearch>;
    friend NODE2;
    friend NODE3;
    friend NODE4;
    SIZE_ASSERT(wordSize);
    //static_assert(sizeof(Node<1, wordSize>) == 64, "size of Node must be 64 byte");
    static_assert(sizeof(unsigned long long) == sizeof(uint64_t));
    static_assert(wordSize * wordSize * 3 % 4 == 0);
    static_assert(sizeof(NODE0) == ((wordSize * wordSize) / 64) * 8);
    static_assert(sizeof(NODE2) == 64, "size of Node must be 64 byte");
    static_assert(sizeof(NODE3) == 64, "size of Node must be 64 byte");
    static_assert(sizeof(NODE4) == 64, "size of Node must be 64 byte");
    static_assert(wordSize * wordSize < 0x3ffe);

private:
    static inline NODE0 *allocateLeaves() {
        ASSERT_CODE_LOCALE(
                auto x = RCNODE_0(std::aligned_alloc(64, wordSize * wordSize * NODE2::numChildren));
                ASSERT_E((rc<uintptr_t>(x) & 0x3f), 0)
                return x;
        )
        return RCNODE_0(std::aligned_alloc(64, wordSize * wordSize * NODE2::numChildren));
    }

    template<uint32_t upperlevel, uint32_t lowerlevel>
    static inline NODE_UN(lowerlevel) *allocateNode() {
        return rc<NODE_UN(lowerlevel) *>(std::aligned_alloc(64, sizeof(NODE_UN(lowerlevel)) * NODE_UN(upperlevel)::numChildren));
    }

    int height = 0;
    uint64_t _nums = 0, _ones = 0;
    uint8_t *root;

    constexpr static pair<int, uint64_t> getMax() {
        uint64_t leaveBits = wordSize * wordSize;
        int out = 1;
        //TODO: wrong formula there can ce 2*assert_var more bits saved.
        while(leaveBits * 15 <= UINT16_MAX)++out, leaveBits *= 15;
        while(leaveBits * 8 <= UINT32_MAX)++out, leaveBits *= 8;
        while(!(leaveBits & 0xc000000000000000ull))++out, leaveBits *= 4;
        return {out, leaveBits};
    }

    static const constexpr std::array<uint64_t, getMax().first> level = []() constexpr {
        int c = 0;
        std::array<uint64_t, getMax().first> out{};
        uint64_t leaveBits = wordSize * wordSize;
        out[c++] = 0;
        while(leaveBits * 15 <= UINT16_MAX)out[c++] = 2, leaveBits *= 15;
        while(leaveBits * 8 <= UINT32_MAX)out[c++] = 3, leaveBits *= 8;
        while(!(leaveBits & 0xc000000000000000ull))out[c++] = 4, leaveBits *= 4;
        return out;
    }();
    static_assert(level[1] == 2);

    template<bool rank, bool one>
    inline uint64_t select_rankIdx(uint64_t key) const {
        //Idee: äquivalent zu access
        uint8_t *curRoot = root;
        int i = height;
        uint64_t out = 0;
        uint_fast8_t idx;
        uint64_t tempOut;
        for(; level[i] == 4; --i) {
            tie(idx, tempOut) = RCNODE_4(curRoot)->template select_rankIdx<rank, one>(key);
            curRoot = rcbyte(rcuint(RCNODE_4(curRoot)->getChildrenPtr()) + (RCNODE_4(curRoot)->toRealIdx(idx) * 64));
            out += tempOut;
        }
        for(; level[i] == 3; --i) {
            tie(idx, tempOut) = RCNODE_3(curRoot)->template select_rankIdx<rank, one>(key);
            curRoot = rcbyte(rcuint(RCNODE_3(curRoot)->getChildrenPtr()) + (RCNODE_3(curRoot)->toRealIdx(idx) * 64));
            out += tempOut;
        }
        for(; i > 1; --i) {
            tie(idx, tempOut) = RCNODE_2(curRoot)->template select_rankIdx<rank, one>(key);
            curRoot = rcbyte(rcuint(RCNODE_2(curRoot)->getChildrenPtr()) + (RCNODE_2(curRoot)->toRealIdx(idx) * 64));
            out += tempOut;
        }
        if(i == 1) {
            tie(idx, tempOut) = (RCNODE_2(curRoot)->template select_rankIdx<rank, one>(key));
            curRoot = rcbyte(RCNODE_0(RCNODE_2(curRoot)->getChildrenPtr()) + (RCNODE_2(curRoot)->toRealIdx(idx)));
            out += tempOut;
        }
        if constexpr(wordSize == 64)__builtin_prefetch(curRoot, 0, 0);
        if constexpr(rank) {
            return out + RCNODE_0(curRoot)->template rank<one>(key);
        } else {
            return out + RCNODE_0(curRoot)->template select<one>(key);
        }
    }

public:

    DynamicBitVector(BitVectorBuilder<wordSize, useBinarySearch> &bv) {
        bv.createTree(this);
    }

    DynamicBitVector() : root(rcbyte(allocateLeaves())) {
    }


    ~DynamicBitVector() {
        //  return;
        if(height == 0) {
            free(root);
            return;
        }
        vector<uintptr_t> queue, toDel;
        queue.reserve(1 << 8);
        queue.push_back(rcuint (root));
        toDel.push_back(rcuint (root));
        size_t begin = 0, end;
        uint_fast8_t curLvl = height;
        for(; curLvl > 1; --curLvl) {
            end = queue.size();
            for(; begin < end; ++begin) {
                switch(level[curLvl]) {
                    case 0:
                    default:
                        ASSERT_UNREACHEABLE(curLvl, level[curLvl])
                    case 2: {
                        NODE2 *lvl2Ptr = RCNODE_2(queue[begin]);
                        toDel.push_back(rcuint(lvl2Ptr->getChildrenPtr()));
                        for(int i = 0; i < lvl2Ptr->size(); ++i) {
                            queue.push_back(rcuint(RCNODE_2(lvl2Ptr->getChildrenPtr()) + lvl2Ptr->toRealIdx(i)));
                        }
                        break;
                    }
                    case 3: {
                        NODE3 *lvl3Ptr = RCNODE_3(queue[begin]);
                        toDel.push_back(rcuint(lvl3Ptr->getChildrenPtr()));
                        for(int i = 0; i < lvl3Ptr->size(); ++i) {
                            queue.push_back(rcuint(RCNODE_3(lvl3Ptr->getChildrenPtr()) + lvl3Ptr->toRealIdx(i)));
                        }
                        break;
                    }
                    case 4: {
                        NODE4 *lvl4Ptr = RCNODE_4(queue[begin]);
                        toDel.push_back(rcuint(lvl4Ptr->getChildrenPtr()));
                        for(int i = 0; i < lvl4Ptr->size(); ++i) {
                            queue.push_back(rcuint(RCNODE_4(lvl4Ptr->getChildrenPtr()) + lvl4Ptr->toRealIdx(i)));
                        }
                        break;
                    }
                }
            }
            ASSERT_E(begin, end)
        }
        ASSERT_E(curLvl, 1)
        end = queue.size();
        for(; begin < end; ++begin) {
            NODE2 *lvl2Ptr = RCNODE_2(queue[begin]);
            free(lvl2Ptr->getChildrenPtr());
        }
        for(uintptr_t x: toDel) {
            free(rcbyte(x));
        }
    }

    uint64_t nums() const {
        return _nums;
    }

    uint64_t ones() const {
        return _ones;
    }


    uint64_t sizeInBytes() const {
        if(height == 0)return sizeof(DynamicBitVector<wordSize, useBinarySearch>) + ((wordSize * wordSize * NODE2::numChildren) / 8);
        vector<uintptr_t> queue;
        uint64_t out = 0;
        // konstanter unbenutzter überschuss an der Wurzel
        switch(level[height + 1]) {
            case 2:
                out += 64 * (NODE2::numChildren - 1);
                break;
            case 3:
                out += 64 * (NODE3::numChildren - 1);
                break;
            case 4:
                out += 64 * (NODE4::numChildren - 1);
                break;
        }
        queue.push_back(rcuint (root));
        size_t begin = 0, end;
        uint_fast8_t curLvl = height;
        //bfs über alle Ebenen ausser untere 2 Ebenen
        //größe eines Knotens ist 64 + die Anzahl unbenutzter Speicherzellen
        for(; curLvl > 1; --curLvl) {
            end = queue.size();
            for(; begin < end; ++begin) {
                switch(level[curLvl]) {
                    case 0:
                    default:
                        ASSERT_UNREACHEABLE(curLvl, level[curLvl])
                    case 2: {
                        NODE2 *lvl2Ptr = RCNODE_2(queue[begin]);
                        out += 64 * (1 + NODE2::numChildren - lvl2Ptr->size());
                        for(int i = 0; i < lvl2Ptr->size(); ++i) {
                            queue.push_back(rcuint(RCNODE_2(lvl2Ptr->getChildrenPtr()) + lvl2Ptr->toRealIdx(i)));
                        }
                        break;
                    }
                    case 3: {
                        NODE3 *lvl3Ptr = RCNODE_3(queue[begin]);
                        out += 64 * (1 + NODE3::numChildren - lvl3Ptr->size());
                        for(int i = 0; i < lvl3Ptr->size(); ++i) {
                            queue.push_back(rcuint(RCNODE_3(lvl3Ptr->getChildrenPtr()) + lvl3Ptr->toRealIdx(i)));
                        }
                        break;
                    }
                    case 4: {
                        NODE4 *lvl4Ptr = RCNODE_4(queue[begin]);
                        out += 64 * (1 + NODE4::numChildren - lvl4Ptr->size());
                        for(int i = 0; i < lvl4Ptr->size(); ++i) {
                            queue.push_back(rcuint(RCNODE_4(lvl4Ptr->getChildrenPtr()) + lvl4Ptr->toRealIdx(i)));
                        }
                        break;
                    }
                }
            }
            ASSERT_E(begin, end)
        }
        end = queue.size();
        //Speicherplatz für vorletzte Ebenen ist 64+ die Ebene ganz unten.
        for(; begin < end; ++begin) {
            out += 64 + (NODE2::numChildren * (wordSize * wordSize) / 8);
        }
        return out + sizeof(DynamicBitVector<wordSize, useBinarySearch>);
    }

    uint_fast8_t access(uint64_t key) {
        //Ist eine ausgerollte DFS
        //Suche in Jedem Knoten das richtige Kind, und gehe eine Ebene tiefer.
        ASSERT_R(key, 0, _nums)
        uint8_t *curRoot = root;
        int i = height;
        for(; level[i] == 4; --i) {
            curRoot = rcbyte(rcuint(RCNODE_4(curRoot)->getChildrenPtr()) + (RCNODE_4(curRoot)->toRealIdx(RCNODE_4(curRoot)->getIdx(key)) * 64));
        }
        for(; level[i] == 3; --i) {
            curRoot = rcbyte(rcuint(RCNODE_3(curRoot)->getChildrenPtr()) + (RCNODE_3(curRoot)->toRealIdx(RCNODE_3(curRoot)->getIdx(key)) * 64));
        }
        for(; i > 1; --i) {
            curRoot = rcbyte(rcuint(RCNODE_2(curRoot)->getChildrenPtr()) + (RCNODE_2(curRoot)->toRealIdx(RCNODE_2(curRoot)->getIdx(key)) * 64));
        }
        if(i == 1) {
            curRoot = rcbyte(RCNODE_0(RCNODE_2(curRoot)->getChildrenPtr()) + RCNODE_2(curRoot)->toRealIdx(RCNODE_2(curRoot)->getIdx(key)));
        }
        return RCNODE_0(curRoot)->access(key);
    }

    template<bool one>
    uint64_t select(uint64_t key) {
        ASSERT_R(key, 0, one ? _ones : _nums - _ones)
        return select_rankIdx<false, one>(key);
    }

    template<bool one>
    uint64_t rank(uint64_t key) {
        ASSERT_R(key, 0, _nums + 1)
        if(key == _nums) {
            if constexpr(one) {
                return _ones;
            } else {
                return _nums - _ones;
            }
        } else {//TODO experimental
            if constexpr(one) {
                return select_rankIdx<true, true>(key);
            } else {
                ASSERT_E((select_rankIdx<true, false>(key)), (key - select_rankIdx<true, true>(key)))
                return key - select_rankIdx<true, true>(key);
            }
        }
    }

    void flip(uint64_t key) {
        //Ausgerollte DFS
        //Suche das richtige Kind äquivalent wie in access aber Speicher alle Kinder in einem Stack.
        //gehe den Stack wieder zurück und ink-/dekrementiere alle Einsen
        ASSERT_R(key, 0, _nums)
        int i = height;
        uint8_t *updateRoots[height + 1]; //stack für die zu updatenden Knoten
        uint_fast8_t indices[height + 1]; //stack für die zugehörigen Indizes
        updateRoots[i] = root;
        for(; level[i] == 4; --i) {
            //            curRoot = rcbyte(rcuint(RCNODE_4(curRoot)->getChildrenPtr()) + (RCNODE_4(curRoot)->getIdx(key) * 64));
            indices[i] = RCNODE_4(updateRoots[i])->getIdx(key);
            updateRoots[i - 1] = rcbyte(rcuint(RCNODE_4(updateRoots[i])->getChildrenPtr()) + (RCNODE_4(updateRoots[i])->toRealIdx(indices[i]) * 64));
        }
        for(; level[i] == 3; --i) {
            indices[i] = RCNODE_3(updateRoots[i])->getIdx(key);
            updateRoots[i - 1] = rcbyte(rcuint(RCNODE_3(updateRoots[i])->getChildrenPtr()) + (RCNODE_3(updateRoots[i])->toRealIdx(indices[i]) * 64));
        }
        for(; i > 1; --i) {
            indices[i] = RCNODE_2(updateRoots[i])->getIdx(key);
            updateRoots[i - 1] = rcbyte(rcuint(RCNODE_2(updateRoots[i])->getChildrenPtr()) + (RCNODE_2(updateRoots[i])->toRealIdx(indices[i]) * 64));
        }
        if(i == 1) {
            indices[i] = RCNODE_2(updateRoots[i])->getIdx(key);
            //RCNODE_0(RCNODE_2(curRoot)->getChildrenPtr()) + (RCNODE_2(curRoot)->getIdx(key));
            updateRoots[i - 1] = rcbyte(RCNODE_0(RCNODE_2(updateRoots[i])->getChildrenPtr()) + (RCNODE_2(updateRoots[i])->toRealIdx(indices[i])));
        }
        ASSERT_CODE_GLOBALE(bool assert_b = RCNODE_0(updateRoots[0])->access(key);)
        RCNODE_0( updateRoots[0])->flip(key);
        const bool b = RCNODE_0( updateRoots[0])->access(key);
        ASSERT_E(assert_b, !b)
        i = height;
        if(b) {
            ++_ones;
            for(; level[i] == 4; --i) {
                RCNODE_4(updateRoots[i])->template incDecOnes<true>(indices[i]);
            }
            for(; level[i] == 3; --i) {
                RCNODE_3(updateRoots[i])->template incDecOnes<true>(indices[i]);
            }
            for(; i >= 1; --i) {
                RCNODE_2(updateRoots[i])->template incDecOnes<true>(indices[i]);
            }
        } else {
            --_ones;
            for(; level[i] == 4; --i) {
                RCNODE_4(updateRoots[i])->template incDecOnes<false>(indices[i]);
            }
            for(; level[i] == 3; --i) {
                RCNODE_3(updateRoots[i])->template incDecOnes<false>(indices[i]);
            }
            for(; i >= 1; --i) {
                RCNODE_2(updateRoots[i])->template incDecOnes<false>(indices[i]);
            }
        }
    }

    template<bool b>
    void insert(uint64_t key) {
        //Idee gehe nach unten und Speicher alle Variablen in einem Stack. (key benötigt eigentlich keinen)
        //versuche unten das Bit einzufügen.
        //falls es nicht geklappt hat, gehe so lange wieder nach oben, bis ein neuer Knoten eingefügt werden konnte,
        //falls alle Knoten (auf dem Pfad) und die Wurzel voll ist, wird der baum größer gemacht.
        //gehe von dem obersten Knoten wieder nach unten. nun kann jeder Knoten auf dem Pfad mind. einen Knoten hinzufügen.
        //am Ende werden alle Zahlen geupdated
        //Anm: ein Knoten wird (falls die Benachbarten quasi voll sind) so geupdated, das aus 3 vollen Knoten, 4 knoten die zu 3/4 voll sind gebaut.
        //     falls die Benachbarten nicht voll sind, werden untere knoten verschoben
        ASSERT_R(key, 0, _nums + 1)
        ASSERT_R(_nums + 1, 0, getMax().second)
        int i = height;
        uint64_t curNums[height + 2] = {}, curOnes[height + 2] = {}, keys[height + 2] = {};
        curNums[i] = nums();
        curOnes[i] = ones();
        ++_nums;
        _ones += b;
        if(i == 0) {
            if(curNums[i] == wordSize * wordSize) {
                NODE2 *nextRoot;
                if constexpr(level[1] == 2) {
                    nextRoot = allocateNode<2, 2>();
                } else {
                    nextRoot = allocateNode<3, 2>();
                }
                const uint16_t newNums[2] = {(wordSize * wordSize) / 2, (wordSize * wordSize) / 2};
                RCNODE_0(root)->moveRight(RCNODE_0(root) + 1, wordSize * wordSize, (wordSize * wordSize) / 2);
                const uint16_t newOnes[2] = {RCNODE_0(root)->template rank<true>((wordSize * wordSize) / 2), (RCNODE_0(root) + 1)->template rank<true>((wordSize * wordSize) / 2)};
                nextRoot->template init<2>(newNums, newOnes);
                curNums[1] = (wordSize * wordSize) / 2;
                curOnes[1] = scu64(newOnes[key > (wordSize * wordSize) / 2]);
                nextRoot->setChildrenPtr(root);
                root = rcbyte(nextRoot);
                height = 1;
                i = 1;
            } else {
                RCNODE_0(root)->template insert<b>(key);
                return;
            }
        }
        uint8_t *updateRoots[height + 2];
        uint_fast8_t indices[height + 2];
        updateRoots[i] = root;
        keys[height] = key;
        for(; level[i] == 4; --i) {
            curNums[i - 1] = curNums[i];
            keys[i - 1] = keys[i];
            indices[i] = RCNODE_4(updateRoots[i])->getIdx(keys[height - 1], curNums[i - 1], curOnes[i - 1]);
            updateRoots[i - 1] = rcbyte(rcuint(RCNODE_4(updateRoots[i])->getChildrenPtr()) + (RCNODE_4(updateRoots[i])->toRealIdx(indices[i]) * 64));
        }
        for(; level[i] == 3; --i) {
            curNums[i - 1] = curNums[i];
            curOnes[i - 1] = curOnes[i];
            keys[i - 1] = keys[i];
            indices[i] = RCNODE_3(updateRoots[i])->getIdx(keys[i - 1], curNums[i - 1], curOnes[i - 1]);
            updateRoots[i - 1] = rcbyte(rcuint(RCNODE_3(updateRoots[i])->getChildrenPtr()) + (RCNODE_3(updateRoots[i])->toRealIdx(indices[i]) * 64));
        }
        for(; i > 1; --i) {
            keys[i - 1] = keys[i];
            curNums[i - 1] = curNums[i];
            curOnes[i - 1] = curOnes[i];
            indices[i] = RCNODE_2(updateRoots[i])->getIdx(keys[i - 1], curNums[i - 1], curOnes[i - 1]);
            updateRoots[i - 1] = rcbyte(rcuint(RCNODE_2(updateRoots[i])->getChildrenPtr()) + (RCNODE_2(updateRoots[i])->toRealIdx(indices[i]) * 64));
        }
        if(i == 1) {
            keys[0] = keys[1];
            curNums[0] = curNums[1];
            curOnes[0] = curOnes[1];
            indices[1] = RCNODE_2(updateRoots[1])->getIdx(keys[0], curNums[0], curOnes[0]);
            updateRoots[0] = rcbyte(RCNODE_0(RCNODE_2(updateRoots[1])->getChildrenPtr()) + (RCNODE_2(updateRoots[1])->toRealIdx(indices[1])));
        }
        if(RCNODE_2(updateRoots[1])->template lowestAllocate<true>(curNums[1], curNums[0], updateRoots[0], keys[0], indices[1])) {
            RCNODE_0(updateRoots[0])->template insert<b>(keys[0]);
            for(int j = 1; j <= height; ++j) {
                switch(level[j]) {
                    case 2:
                        RCNODE_2(updateRoots[j])->template incDecNums<true>(indices[j]);
                        if constexpr(b) RCNODE_2(updateRoots[j])->template incDecOnes<true>(indices[j]);
                        break;
                    case 3:
                        RCNODE_3(updateRoots[j])->template incDecNums<true>(indices[j]);
                        if constexpr(b) RCNODE_3(updateRoots[j])->template incDecOnes<true>(indices[j]);
                        break;
                    case 4:
                        RCNODE_4(updateRoots[j])->template incDecNums<true>(indices[j]);
                        if constexpr(b) RCNODE_4(updateRoots[j])->template incDecOnes<true>(indices[j]);
                        break;
                    default:
                        ASSERT_UNIMPLEMETED();
                }
            }
        } else {
            for(i = 2; (i <= height) & (level[i] == 2); ++i) {
                if(level[i - 2] == 0) {
                    if(RCNODE_2(updateRoots[i])->template allocate<true, 2, true>(updateRoots[1], indices[2], indices[1], curNums[2], curOnes[2], curNums[1], curOnes[1])) {
                        updateRoots[0] = rcbyte(RCNODE_0(RCNODE_2(updateRoots[1])->getChildrenPtr()) + (RCNODE_2(updateRoots[1])->toRealIdx(indices[1])));
                        goto notMaxHeightReached;
                    }
                } else {
                    if(RCNODE_2(updateRoots[i])->template allocate<true, 2, false>(updateRoots[i - 1], indices[i], indices[i - 1], curNums[i], curOnes[i], curNums[i - 1], curOnes[i - 1])) {
                        updateRoots[i - 2] = rcbyte((RCNODE_2(updateRoots[i - 1])->getChildrenPtr()) + (RCNODE_2(updateRoots[i - 1])->toRealIdx(indices[i - 1]) * 64));
                        goto notMaxHeightReached;
                    }
                }
            }
            for(; (i <= height) & (level[i] == 3); ++i) {
                if constexpr(level[2] == 3) {
                    if(level[i - 1] == 2) {
                        if(RCNODE_3(updateRoots[i])->template allocate<true, 2, true>(updateRoots[1], indices[2], indices[1], curNums[2], curOnes[2], curNums[1], curOnes[1])) {
                            updateRoots[0] = rcbyte(RCNODE_0(RCNODE_2(updateRoots[1])->getChildrenPtr()) + (RCNODE_2(updateRoots[1])->toRealIdx(indices[1])));
                            goto notMaxHeightReached;
                        }
                    } else {
                        if(RCNODE_3(updateRoots[i])->template allocate<true, 3, false>(updateRoots[i - 1], indices[i], indices[i - 1], curNums[i], curOnes[i], curNums[i - 1], curOnes[i - 1])) {
                            updateRoots[i - 2] = rcbyte((RCNODE_3(updateRoots[i - 1])->getChildrenPtr()) + (RCNODE_3(updateRoots[i - 1])->toRealIdx(indices[i - 1]) * 64));
                            goto notMaxHeightReached;
                        }
                    }
                } else {
                    if(level[i - 1] == 2) {
                        if(RCNODE_3(updateRoots[i])->template allocate<true, 2, false>(updateRoots[i - 1], indices[i], indices[i - 1], curNums[i], curOnes[i], curNums[i - 1], curOnes[i - 1])) {
                            updateRoots[i - 2] = rcbyte((RCNODE_2(updateRoots[i - 1])->getChildrenPtr()) + (RCNODE_2(updateRoots[i - 1])->toRealIdx(indices[i - 1]) * 64));
                            goto notMaxHeightReached;
                        }
                    } else {
                        if(RCNODE_3(updateRoots[i])->template allocate<true, 3, false>(updateRoots[i - 1], indices[i], indices[i - 1], curNums[i], curOnes[i], curNums[i - 1], curOnes[i - 1])) {
                            updateRoots[i - 2] = rcbyte((RCNODE_3(updateRoots[i - 1])->getChildrenPtr()) + (RCNODE_3(updateRoots[i - 1])->toRealIdx(indices[i - 1]) * 64));
                            goto notMaxHeightReached;
                        }
                    }
                }
            }
            for(; (i <= height) & (level[i] == 4); ++i) {
                if(level[i - 1] == 3) {
                    if(RCNODE_4(updateRoots[i])->template allocate<true, 3, false>(updateRoots[i - 1], indices[i], indices[i - 1], curNums[i], curOnes[i], curNums[i - 1], curOnes[i - 1])) {
                        updateRoots[i - 2] = rcbyte((RCNODE_3(updateRoots[i - 1])->getChildrenPtr()) + (RCNODE_3(updateRoots[i - 1])->toRealIdx(indices[i - 1]) * 64));
                        goto notMaxHeightReached;
                    }
                } else {
                    if(RCNODE_4(updateRoots[i])->template allocate<true, 4, false>(updateRoots[i - 1], indices[i], indices[i - 1], curNums[i], curOnes[i], curNums[i - 1], curOnes[i - 1])) {
                        updateRoots[i - 2] = rcbyte((RCNODE_4(updateRoots[i - 1])->getChildrenPtr()) + (RCNODE_4(updateRoots[i - 1])->toRealIdx(indices[i - 1]) * 64));
                        goto notMaxHeightReached;
                    }
                }
            }
            //ASSERT_E(i, height + 1)
            ++height;
            indices[height] = 0;
            switch(level[height]) {
                case 2: {
                    ASSERT_E(scu32(RCNODE_2(root)->size()), NODE2::numChildren)
                    NODE2 *nextRoot = (level[height + 1] == 2) ? allocateNode<2, 2>() : allocateNode<3, 2>();
                    uint16_t nextNums[2] = {curNums[height - 1], 0}, nextOnes[2] = {curOnes[height - 1], 0};
                    if(level[height - 2] == 0) {
                        RCNODE_2(RCNODE_2(root) + 1)->template moveInit<uint16_t, true>(RCNODE_2(root), NODE2::numChildren / 2, NODE2::numChildren, updateRoots[height - 1], indices[height], indices[height - 1], nextNums[0], nextNums[1], nextOnes[0], nextOnes[1]);
                        updateRoots[height - 2] = rcbyte(rcuint(RCNODE_2(updateRoots[height - 1])->getChildrenPtr()) + (RCNODE_2(updateRoots[height - 1])->toRealIdx(indices[height - 1]) * sizeof(NODE0)));
                    } else {
                        RCNODE_2(RCNODE_2(root) + 1)->template moveInit<uint16_t, false>(RCNODE_2(root), NODE2::numChildren / 2, NODE2::numChildren, updateRoots[height - 1]/*, updateRoots[height - 1]*/, indices[height], indices[height - 1], nextNums[0], nextNums[1], nextOnes[0], nextOnes[1]);
                        updateRoots[height - 2] = rcbyte(rcuint(RCNODE_2(updateRoots[height - 1])->getChildrenPtr()) + (RCNODE_2(updateRoots[height - 1])->toRealIdx(indices[height - 1]) * 64));
                    }
                    ASSERT_R(indices[height], 0, 2)
                    curNums[height - 1] = nextNums[indices[height]];
                    curOnes[height - 1] = nextOnes[indices[height]];
                    nextRoot->template init<2>(nextNums, nextOnes);
                    nextRoot->setChildrenPtr(root);
                    root = rcbyte(nextRoot);
                    updateRoots[height] = root;
                    //TODO update i?
                    break;
                }
                case 3: {
                    ASSERT_PE(level[height - 1] == 2, scu32(RCNODE_2(root)->size()), NODE2::numChildren)
                    ASSERT_PE(level[height - 1] == 3, scu32(RCNODE_3(root)->size()), NODE3::numChildren)
                    NODE3 *nextRoot = (level[height + 1] == 3) ? allocateNode<3, 3>() : allocateNode<4, 3>();
                    uint32_t nextNums[2] = {curNums[height - 1], 0}, nextOnes[2] = {curOnes[height - 1], 0};
                    if constexpr(level[2] == 3) {
                        if(level[height - 2] == 0) {
                            ASSERT_E(level[height - 1], 2)
                            RCNODE_2(RCNODE_2(root) + 1)->template moveInit<uint32_t, true>(RCNODE_2(root), NODE2::numChildren / 2, NODE2::numChildren, updateRoots[height - 1], indices[height], indices[height - 1], nextNums[0], nextNums[1], nextOnes[0], nextOnes[1]);
                            updateRoots[height - 2] = rcbyte(rcuint(RCNODE_2(updateRoots[height - 1])->getChildrenPtr()) + (RCNODE_2(updateRoots[height - 1])->toRealIdx(indices[height - 1]) * sizeof(NODE0)));
                        } else {
                            RCNODE_3(RCNODE_3(root) + 1)->template moveInit<uint32_t, false>(RCNODE_3(root), NODE3::numChildren / 2, NODE3::numChildren, updateRoots[height - 1], indices[height], indices[height - 1], nextNums[0], nextNums[1], nextOnes[0], nextOnes[1]);
                            updateRoots[height - 2] = rcbyte(rcuint(RCNODE_3(updateRoots[height - 1])->getChildrenPtr()) + (RCNODE_3(updateRoots[height - 1])->toRealIdx(indices[height - 1]) * 64));
                        }
                    } else {
                        if(level[height - 1] == 2) {
                            RCNODE_2(RCNODE_2(root) + 1)->template moveInit<uint32_t, false>(RCNODE_2(root), NODE2::numChildren / 2, NODE2::numChildren, updateRoots[height - 1], indices[height], indices[height - 1], nextNums[0], nextNums[1], nextOnes[0], nextOnes[1]);
                            updateRoots[height - 2] = rcbyte(rcuint(RCNODE_2(updateRoots[height - 1])->getChildrenPtr()) + (RCNODE_2(updateRoots[height - 1])->toRealIdx(indices[height - 1]) * 64));
                        } else {
                            RCNODE_3(RCNODE_3(root) + 1)->template moveInit<uint32_t, false>(RCNODE_3(root), NODE3::numChildren / 2, NODE3::numChildren, updateRoots[height - 1], indices[height], indices[height - 1], nextNums[0], nextNums[1], nextOnes[0], nextOnes[1]);
                            updateRoots[height - 2] = rcbyte(rcuint(RCNODE_3(updateRoots[height - 1])->getChildrenPtr()) + (RCNODE_3(updateRoots[height - 1])->toRealIdx(indices[height - 1]) * 64));
                        }
                    }
                    ASSERT_R(indices[height], 0, 2)
                    curNums[height - 1] = nextNums[indices[height]];
                    curOnes[height - 1] = nextOnes[indices[height]];
                    nextRoot->template init<2>(nextNums, nextOnes);
                    nextRoot->setChildrenPtr(root);
                    root = rcbyte(nextRoot);
                    updateRoots[height] = root;
                    break;
                }
                case 4: {
                    ASSERT_PE(level[height - 1] == 3, scu32(RCNODE_3(root)->size()), NODE3::numChildren)
                    ASSERT_PE(level[height - 1] == 4, scu32(RCNODE_4(root)->size()), NODE4::numChildren)
                    NODE4 *nextRoot = allocateNode<4, 4>();
                    uint64_t nextNums[2] = {curNums[height - 1], 0}, nextOnes[2] = {curOnes[height - 1], 0};
                    if(level[height - 1] == 3) {
                        RCNODE_3(RCNODE_3(root) + 1)->template moveInit<uint64_t, false>(RCNODE_3(root), NODE3::numChildren / 2, NODE3::numChildren, updateRoots[height - 1], indices[height], indices[height - 1], nextNums[0], nextNums[1], nextOnes[0], nextOnes[1]);
                        updateRoots[height - 2] = rcbyte(rcuint(RCNODE_3(updateRoots[height - 1])->getChildrenPtr()) + (RCNODE_3(updateRoots[height - 1])->toRealIdx(indices[height - 1]) * 64));
                    } else {
                        RCNODE_4(RCNODE_4(root) + 1)->template moveInit<uint64_t, false>(RCNODE_4(root), NODE4::numChildren / 2, NODE4::numChildren, updateRoots[height - 1], indices[height], indices[height - 1], nextNums[0], nextNums[1], nextOnes[0], nextOnes[1]);
                        updateRoots[height - 2] = rcbyte(rcuint(RCNODE_4(updateRoots[height - 1])->getChildrenPtr()) + (RCNODE_4(updateRoots[height - 1])->toRealIdx(indices[height - 1]) * 64));
                    }
                    ASSERT_R(indices[height], 0, 2)
                    curNums[height - 1] = nextNums[indices[height]];
                    curOnes[height - 1] = nextOnes[indices[height]];
                    nextRoot->template init<2>(nextNums, nextOnes);
                    nextRoot->setChildrenPtr(root);
                    root = rcbyte(nextRoot);
                    updateRoots[height] = root;
                    break;
                }
            }
            notMaxHeightReached:
            --i;
            for(; level[i] == 4; --i) {
                if(level[i - 1] == 3) {
                    RCNODE_4(updateRoots[i])->template allocate<false, 3, false>(updateRoots[i - 1], indices[i], indices[i - 1], curNums[i], curOnes[i], curNums[i - 1], curOnes[i - 1]);
                    updateRoots[i - 2] = rcbyte((RCNODE_3(updateRoots[i - 1])->getChildrenPtr()) + (RCNODE_3(updateRoots[i - 1])->toRealIdx(indices[i - 1]) * 64));
                } else {
                    RCNODE_4(updateRoots[i])->template allocate<false, 4, false>(updateRoots[i - 1], indices[i], indices[i - 1], curNums[i], curOnes[i], curNums[i - 1], curOnes[i - 1]);
                    updateRoots[i - 2] = rcbyte((RCNODE_4(updateRoots[i - 1])->getChildrenPtr()) + (RCNODE_4(updateRoots[i - 1])->toRealIdx(indices[i - 1]) * 64));
                }
            }
            for(; level[i] == 3; --i) {
                if constexpr(level[2] == 3) {
                    if(level[i - 1] == 2) {
                        RCNODE_3(updateRoots[2])->template allocate<false, 2, true>(updateRoots[1], indices[2], indices[1], curNums[2], curOnes[2], curNums[1], curOnes[1]);
                        updateRoots[0] = rcbyte(RCNODE_0(RCNODE_2(updateRoots[1])->getChildrenPtr()) + (RCNODE_2(updateRoots[1])->toRealIdx(indices[1])));
                    } else {
                        RCNODE_3(updateRoots[i])->template allocate<false, 3, false>(updateRoots[i - 1], indices[i], indices[i - 1], curNums[i], curOnes[i], curNums[i - 1], curOnes[i - 1]);
                        updateRoots[i - 2] = rcbyte((RCNODE_3(updateRoots[i - 1])->getChildrenPtr()) + (RCNODE_3(updateRoots[i - 1])->toRealIdx(indices[i - 1]) * 64));
                    }
                } else {
                    if(level[i - 1] == 2) {
                        RCNODE_3(updateRoots[i])->template allocate<false, 2, false>(updateRoots[i - 1], indices[i], indices[i - 1], curNums[i], curOnes[i], curNums[i - 1], curOnes[i - 1]);
                        updateRoots[i - 2] = rcbyte((RCNODE_2(updateRoots[i - 1])->getChildrenPtr()) + (RCNODE_2(updateRoots[i - 1])->toRealIdx(indices[i - 1]) * 64));
                    } else {
                        RCNODE_3(updateRoots[i])->template allocate<false, 3, false>(updateRoots[i - 1], indices[i], indices[i - 1], curNums[i], curOnes[i], curNums[i - 1], curOnes[i - 1]);
                        updateRoots[i - 2] = rcbyte((RCNODE_3(updateRoots[i - 1])->getChildrenPtr()) + (RCNODE_3(updateRoots[i - 1])->toRealIdx(indices[i - 1]) * 64));
                    }
                }
            }
            for(; i > 1; --i) {
                if(level[i - 2] == 0) {
                    RCNODE_2(updateRoots[i])->template allocate<false, 2, true>(updateRoots[i - 1], indices[i], indices[i - 1], curNums[i], curOnes[i], curNums[i - 1], curOnes[i - 1]);
                    updateRoots[0] = rcbyte(RCNODE_0(RCNODE_2(updateRoots[1])->getChildrenPtr()) + (RCNODE_2(updateRoots[1])->toRealIdx(indices[1])));
                } else {
                    RCNODE_2(updateRoots[i])->template allocate<false, 2, false>(updateRoots[i - 1], indices[i], indices[i - 1], curNums[i], curOnes[i], curNums[i - 1], curOnes[i - 1]);
                    updateRoots[i - 2] = rcbyte((RCNODE_2(updateRoots[i - 1])->getChildrenPtr()) + (RCNODE_2(updateRoots[i - 1])->toRealIdx(indices[i - 1]) * 64));
                }
            }
            if(i == 1) {
                RCNODE_2(updateRoots[1])->template lowestAllocate<false>(curNums[1], curNums[0], updateRoots[0], keys[0], indices[1]);
            }
            RCNODE_0(updateRoots[0])->template insert<b>(keys[0]);
            for(int j = 1; j <= height; ++j) {
                switch(level[j]) {
                    case 2:
                        RCNODE_2(updateRoots[j])->template incDecNums<true>(indices[j]);
                        if constexpr(b) RCNODE_2(updateRoots[j])->template incDecOnes<true>(indices[j]);
                        break;
                    case 3:
                        RCNODE_3(updateRoots[j])->template incDecNums<true>(indices[j]);
                        if constexpr(b) RCNODE_3(updateRoots[j])->template incDecOnes<true>(indices[j]);
                        break;
                    default:
                        ASSERT_UNIMPLEMETED();
                }
            }
            // ASSERT_B(std::equal(curNums + 1, curNums + height + 1, assert_curNums + 1))
            // ASSERT_B(std::equal(curOnes + 1, curOnes + height + 1, assert_curOnes + 1))
            // ASSERT_B(std::equal(updateRoots + 1, updateRoots + height + 1, assert_updateRoots + 1))
            // ASSERT_B(std::equal(indices + 1, indices + height + 1, assert_indices + 1))
            //ASSERT_UNREACHEABLE()
        }
    }

    void remove(uint64_t key) {
        //Idee: gehe nach unten, und lösche das Bit.
        //1. falls nun der Knoten in der zweituntersten Ebene und ein Benachbarter gemerged werden könne, merge diese.
        //2. gehe so weit nach oben, bis kein Knoten mehr gemerged werden kann
        //3. update alle Zahlen.
        // (Ein Knoten kann nie leer werden, da die Summe über die Knotengrade aller benachbarten Kindern größer ist, als der Knotengrad eines maximal befüllten Knotens.)
        ASSERT_R(key, 0, _nums)
        ASSERT_R(_nums + 1, 0, getMax().second)
        int i = height;
        uint64_t curNums[height + 2] = {}, curOnes[height + 2] = {}, keys[height + 2] = {};
        curNums[i] = nums();
        curOnes[i] = ones();
        if(i == 0) {
            if(curNums[i] == 0) {
                return;
            } else {
                _ones -= RCNODE_0(root)->access(key);
                --_nums;
                RCNODE_0(root)->remove(key);
                return;
            }
        }
        uint8_t *updateRoots[height + 2];
        uint_fast8_t indices[height + 2];
        updateRoots[i] = root;
        keys[height] = key;
        for(; level[i] == 4; --i) {
            curNums[i - 1] = curNums[i];
            keys[i - 1] = keys[i];
            indices[i] = RCNODE_4(updateRoots[i])->getIdx(keys[height - 1], curNums[i - 1], curOnes[i - 1]);
            updateRoots[i - 1] = rcbyte(rcuint(RCNODE_4(updateRoots[i])->getChildrenPtr()) + (RCNODE_4(updateRoots[i])->toRealIdx(indices[i]) * 64));
        }
        for(; level[i] == 3; --i) {
            curNums[i - 1] = curNums[i];
            curOnes[i - 1] = curOnes[i];
            keys[i - 1] = keys[i];
            indices[i] = RCNODE_3(updateRoots[i])->getIdx(keys[i - 1], curNums[i - 1], curOnes[i - 1]);
            updateRoots[i - 1] = rcbyte(rcuint(RCNODE_3(updateRoots[i])->getChildrenPtr()) + (RCNODE_3(updateRoots[i])->toRealIdx(indices[i]) * 64));
        }
        for(; i > 1; --i) {
            keys[i - 1] = keys[i];
            curNums[i - 1] = curNums[i];
            curOnes[i - 1] = curOnes[i];
            indices[i] = RCNODE_2(updateRoots[i])->getIdx(keys[i - 1], curNums[i - 1], curOnes[i - 1]);
            updateRoots[i - 1] = rcbyte(rcuint(RCNODE_2(updateRoots[i])->getChildrenPtr()) + (RCNODE_2(updateRoots[i])->toRealIdx(indices[i]) * 64));
        }
        if(i == 1) {
            keys[0] = keys[1];
            curNums[0] = curNums[1];
            curOnes[0] = curOnes[1];
            indices[1] = RCNODE_2(updateRoots[1])->getIdx(keys[0], curNums[0], curOnes[0]);
            updateRoots[0] = rcbyte(RCNODE_0(RCNODE_2(updateRoots[1])->getChildrenPtr()) + (RCNODE_2(updateRoots[1])->toRealIdx(indices[1])));
        }
        const bool b = RCNODE_0( updateRoots[0])->access(keys[0]);
        _ones -= b;
        --_nums;
        ASSERT_E(i, 1)
        if(!RCNODE_2(updateRoots[1])->lowestDeallocate(b, curNums[1], curNums[0], keys[0], indices[1]))goto noMoreDelete;
        for(++i; level[i] == 2 && i <= height; ++i) {
            if(level[i - 2] == 0) {
                if(!RCNODE_2(updateRoots[i])->template deallocate<true, 2>(b, curNums[i + 1], curOnes[i + 1], curNums[i], curOnes[i], indices[i]))goto noMoreDelete;
            } else {
                if(!RCNODE_2(updateRoots[i])->template deallocate<false, 2>(b, curNums[i + 1], curOnes[i + 1], curNums[i], curOnes[i], indices[i]))goto noMoreDelete;
            }
        }
        for(; level[i] == 3 && i <= height; ++i) {
            if constexpr(level[2] == 3) {
                if(level[i - 1] == 2) {
                    if(!RCNODE_3(updateRoots[i])->template deallocate<true, 2>(b, curNums[i + 1], curOnes[i + 1], curNums[i], curOnes[i], indices[i]))goto noMoreDelete;
                } else {
                    if(!RCNODE_3(updateRoots[i])->template deallocate<false, 3>(b, curNums[i + 1], curOnes[i + 1], curNums[i], curOnes[i], indices[i]))goto noMoreDelete;
                }
            } else {
                if(level[i - 1] == 2) {
                    if(!RCNODE_3(updateRoots[i])->template deallocate<false, 2>(b, curNums[i + 1], curOnes[i + 1], curNums[i], curOnes[i], indices[i]))goto noMoreDelete;
                } else {
                    if(!RCNODE_3(updateRoots[i])->template deallocate<false, 3>(b, curNums[i + 1], curOnes[i + 1], curNums[i], curOnes[i], indices[i]))goto noMoreDelete;
                }
            }
        }
        for(; level[i] == 4 && i <= height; ++i) {
            if(level[i - 1] == 3) {
                if(!RCNODE_4(updateRoots[i])->template deallocate<false, 3>(b, curNums[i + 1], curOnes[i + 1], curNums[i], curOnes[i], indices[i]))goto noMoreDelete;
            } else {
                if(!RCNODE_4(updateRoots[i])->template deallocate<false, 4>(b, curNums[i + 1], curOnes[i + 1], curNums[i], curOnes[i], indices[i]))goto noMoreDelete;
            }
        }
        ASSERT_E(i, height)
        ASSERT_PE(level[i] == 4, RCNODE_4(root)->size(), 1)
        ASSERT_PE(level[i] == 3, RCNODE_3(root)->size(), 1)
        ASSERT_PE(level[i] == 2, RCNODE_2(root)->size(), 1)
        free(root);
        noMoreDelete:
        ++i;
        for(; i <= height; ++i) {
            switch(level[i]) {
                case 2:
                    RCNODE_2(updateRoots[i])->template incDecNums<false>(indices[i]);
                    if(b) RCNODE_2(updateRoots[i])->template incDecOnes<false>(indices[i]);
                    break;
                case 3:
                    RCNODE_3(updateRoots[i])->template incDecNums<false>(indices[i]);
                    if(b) RCNODE_3(updateRoots[i])->template incDecOnes<false>(indices[i]);
                    break;
                case 4:
                    RCNODE_4(updateRoots[i])->template incDecNums<false>(indices[i]);
                    if(b) RCNODE_4(updateRoots[i])->template incDecOnes<false>(indices[i]);
                    break;
                default: {
                    ASSERT_UNREACHEABLE();
                }
            }
        }
    }
};

template<uint32_t curLevel, WordSizes wordSize, bool useBinarySearch>
class Node {
    SIZE_ASSERT(wordSize);
    static_assert(RANGE(curLevel, 2, 5), "Current Level must be in [2,4]");
public:
};

template<WordSizes wordSize, bool useBinarySearch>
class NODE0 {
    friend NODE2;//TODO remove
    //friend DynamicBitVector<wordSize, useBinarySearch>;
    SIZE_ASSERT(wordSize)
    std::array<uint64_t, (wordSize * wordSize) / 64> bytes;


public:

    Node() = default;

    Node(std::array<uint64_t, (wordSize * wordSize) / 64> &&arr) : bytes(std::forward<std::array<uint64_t, (wordSize * wordSize) / 64>>(arr)) {

    };

    template<bool one>
    inline uint32_t rank(uint32_t key) const {
        ASSERT_R(key, 0, wordSize * wordSize)
        uint32_t out = 0;
        ASSERT_CODE_GLOBALE(uint64_t assert_num = bytes[0];)
        if constexpr(one) {
            int i = 0;
            for(; i < (key / 64); ++i) {
                out += __builtin_popcountll(bytes[i]);
            }
            out += __builtin_popcountll(bytes[i] & ((scu64(1) << (key % 64)) - 1));
        } else {
            int i = 0;
            for(; i < key / 64; ++i) {
                out += __builtin_popcountll(~bytes[i]);
            }
            out += __builtin_popcountll((~bytes[i]) & ((scu64(1) << (key % 64)) - 1));
        }
        ASSERT_E(bytes[0], assert_num);
        return out;
    }

    template<bool one>
    inline uint32_t select(uint32_t key) const {
        ASSERT_CODE_GLOBALE(int assert_c = 0;
                                    for(int i = 0; i < bytes.size(); ++i)
                                        assert_c += __builtin_popcountll(one ? bytes[i] : ~bytes[i]);
        )
        ASSERT_R(key, 0, assert_c, one)
        uint32_t out = 0;
        int i = 0;
        //uint64_t *pos = bytes;
        if constexpr(one) {
            for(int temp; key >= (temp = __builtin_popcountll(bytes[i])); ++i)
                key -= temp, out += 64;
            out += __builtin_ctzll(__builtin_ia32_pdep_di(scu64(1) << key, bytes[i]));
        } else {
            for(int temp; key >= (temp = __builtin_popcountll(~(bytes[i]))); ++i)
                key -= temp, out += 64;
            out += __builtin_ctzll(__builtin_ia32_pdep_di(scu64(1) << key, ~(bytes[i])));

        }
        return out;
    }

    inline bool access(uint32_t key) const {
        ASSERT_R(key, 0, wordSize * wordSize)
        return (bytes[key / 64] & (scu64(1) << (key % 64))) != scu64(0);
    }

    inline void flip(uint32_t key) {
        ASSERT_R(key, 0, wordSize * wordSize)
        bytes[key / 64] ^= (scu64(1) << (key % 64));
    }

    template<bool b>
    inline void insert(uint32_t key) {
        ASSERT_R(key, 0, wordSize * wordSize)
        uint_fast8_t i = (key / 64);
        uint64_t overflow = (bytes[i]) >> 63;
        bytes[i] = __builtin_ia32_pdep_di(bytes[i], ~(scu64(1) << (key % 64)));
        bytes[i] |= scu64(b) << (key % 64);
        for(++i; i < bytes.size(); ++i) {
            bytes[i] = __rolq(bytes[i], 1);
            overflow ^= bytes[i] & 1;
            bytes[i] ^= overflow;
            overflow ^= bytes[i] & 1;
        }
    }

    inline void remove(uint32_t key) {
        ASSERT_R(key, 0, wordSize * wordSize)
        uint_fast8_t i = bytes.size() - 1;
        uint64_t overflow = 0;
        for(; i > (key / 64); --i) {
            overflow ^= bytes[i] & 1;
            bytes[i] ^= overflow;
            overflow ^= bytes[i] & 1;
            bytes[i] = __rorq(bytes[i], 1);
        }
        ASSERT_E(i, key / 64)
        bytes[i] = (overflow << 63) | __builtin_ia32_pext_di(bytes[i], ~(scu64(1) << (key % 64)));
    }

    inline void moveRight(NODE0 *rNode, uint32_t thisSize, uint32_t delta) {
        ASSERT_NE(delta, 0)
        //ASSERT_E(rNode, this + 1)
        ASSERT_NE(rNode, this)
        const int_fast8_t idxKey = delta / 64, restKey = delta % 64;
        const uint64_t bitmask = (scu64(1) << (restKey)) - 1;
        int_fast8_t i = (rNode->bytes.size() - 1);
        if(idxKey > 0) {
            for(; i >= idxKey; --i) {
                rNode->bytes[i] = rNode->bytes[i - (idxKey)];
            }
            for(; i >= 0; --i) {
                rNode->bytes[i] = 0;
            }
        }
        i = idxKey;
        uint64_t overflow = __rolq(rNode->bytes[i], restKey) & bitmask;
        rNode->bytes[i] = (rNode->bytes[i] << restKey);
        for(++i; i < rNode->bytes.size(); ++i) {
            rNode->bytes[i] = __rolq(rNode->bytes[i], restKey);
            overflow ^= rNode->bytes[i] & bitmask;
            rNode->bytes[i] ^= overflow;
            overflow ^= rNode->bytes[i] & bitmask;
        }
        ASSERT_PE(delta != wordSize * wordSize, rNode->rank<true>(delta), 0)
        ASSERT_PE(delta == wordSize * wordSize, std::accumulate(rNode->bytes.begin(), rNode->bytes.end(), scu64(0), [](uint64_t x, uint64_t y) { return x + __builtin_popcountll(y); }), 0)
        uint_fast8_t idxKey2 = (thisSize - delta) / 64;
        const uint_fast8_t restKey2 = (thisSize - delta) % 64;
        const uint64_t bitmask2 = (scu64(1) << (restKey2)) - 1;
        for(i = 0; i < idxKey; ++i, ++idxKey2) {
            rNode->bytes[i] |= (bytes[idxKey2] >> restKey2);
            rNode->bytes[i] |= __rorq(bytes[idxKey2 + 1] & bitmask2, restKey2);
        }
        if(restKey > 0) {
            rNode->bytes[i] |= (bytes[idxKey2] >> restKey2);
        }
        if(restKey + restKey2 > 64) {
            rNode->bytes[i] |= __rorq(bytes[idxKey2 + 1] & bitmask2, restKey2);
        }

    }

    inline void moveLeft(NODE0 *lNode, uint32_t leftSize, uint32_t delta) {
        ASSERT_NE(delta, 0)
        ASSERT_NE(lNode, this)
        const uint_fast8_t idxKey = delta / 64, restKey = delta % 64;
        const uint64_t bitmask = (scu64(1) << (restKey)) - 1;
        int_fast8_t i = 0;
        uint_fast8_t idxKey2 = (leftSize) / 64;
        const uint_fast8_t restKey2 = (leftSize) % 64;
        const uint64_t bitmask2 = (scu64(1) << (restKey2)) - 1;
        lNode->bytes[idxKey2] &= bitmask2;
        for(i = idxKey2 + 1; i < bytes.size(); ++i) {
            lNode->bytes[i] = 0;
        }
        ASSERT_E(lNode->rank<true>(leftSize), std::accumulate(lNode->bytes.begin(), lNode->bytes.end(), scu64(0), [](uint64_t x, uint64_t y) { return x + __builtin_popcountll(y); }))
        for(i = 0; i < idxKey; ++i, ++idxKey2) {
            lNode->bytes[idxKey2] |= (bytes[i] << restKey2);
            lNode->bytes[idxKey2 + 1] |= __rolq(bytes[i], restKey2) & bitmask2;
        }
        if(restKey > 0) {
            lNode->bytes[idxKey2] |= (bytes[i] << restKey2);
        }
        if(restKey + restKey2 > 64) {
            lNode->bytes[idxKey2 + 1] |= __rolq(bytes[i], restKey2) & bitmask2;
        }

        if(idxKey > 0) {
            for(i = 0; i < (bytes.size() - idxKey); ++i) {
                bytes[i] = bytes[i + (idxKey)];
            }
        }
        uint64_t overflow = 0;
        for(i = (bytes.size() - idxKey) - 1; i >= 0; --i) {
            overflow ^= bytes[i] & bitmask;
            bytes[i] ^= overflow;
            overflow ^= bytes[i] & bitmask;
            bytes[i] = __rorq(bytes[i], restKey);
        }

    }

    static void test() {
        std::array<uint64_t, (wordSize * wordSize) / 64> arr = {};
        NODE0 node(std::move(arr));
        ASSERT_CODE_LOCALE(for(auto x: node.bytes) {
            ASSERT_E(x, 0)
        })
        vector<bool> bits;
        for(int i = 0; i < wordSize * wordSize; ++i) {
            const bool b = rand() % 2;
            const int idx = rand() % (i + 1);
            bits.insert(std::next(bits.begin(), idx), b);
            if(b) {
                node.template insert<true>(idx);
            } else {
                node.template insert<false>(idx);
            }
            for(int j = 0; j < i; ++j) {
                ASSERT_E(node.access(j), bits[j])
            }
        }
        for(int j = 0; j < wordSize * wordSize; ++j) {
            ASSERT_E(node.access(j), bits[j])
        }
        for(int i = 0; i < 10000; ++i) {
            const int idx = rand() % (wordSize * wordSize);
            node.flip(idx);
            bits[idx] = !bits[idx];
        }
        for(int i = 0; i < wordSize * wordSize; ++i) {
            ASSERT_E(node.access(i), bits[i]);
        }
        for(int i = 0; i < wordSize * wordSize; ++i) {
            ASSERT_E(node.template rank<true>(i),
                     std::accumulate(bits.begin(), bits.begin() + i, scu32(0),
                                     [](uint32_t x, bool y) { return x + y; }))
            ASSERT_E(node.template rank<false>(i),
                     std::accumulate(bits.begin(), bits.begin() + i, scu32(0),
                                     [](uint32_t x, bool y) { return x + (!y); }))
        }
        const uint32_t ones = node.template rank<true>(wordSize * wordSize - 1), zeros = node.template rank<false>(wordSize * wordSize - 1);
        ASSERT_E(zeros + ones, wordSize * wordSize - 1)
        for(uint32_t i = 0; i < ones; ++i) {
            int temp = i + 1, j = 0;
            for(; j < wordSize * wordSize; ++j) {
                if(bits[j]) {
                    --temp;
                }
                if(temp == 0)break;
            }
            ASSERT_E(scu32(node.template select<true>(i)), scu32(j))
        }
        for(uint32_t i = 0; i < zeros; ++i) {
            int temp = i + 1, j = 0;
            for(; j < wordSize * wordSize; ++j) {
                if(!bits[j]) {
                    --temp;
                }
                if(temp == 0)break;
            }
            ASSERT_E(scu32(node.template select<false>(i)), scu32(j))
        }
        for(int i = wordSize * wordSize; i > 0; --i) {
            const int idx = rand() % i;
            node.remove(idx);
            bits.erase(std::next(bits.begin(), idx));
            for(int j = 0; j < (i - 1); ++j) {
                ASSERT_E(node.access(j), bits[j])
            }
        }
        for(int i = 0; i < node.bytes.size(); ++i) {
            ASSERT_E(node.bytes[i], 0)
        }
        for(int usedSize = 0; usedSize <= (wordSize * wordSize); ++usedSize) {
            for(int move = 0; move < usedSize; ++move) {
                bits.clear();
                NODE0 nodes[2];
                const int invMove = usedSize - (move);
                for(int i = 0; i < ((wordSize * wordSize) / 64); ++i) {
                    nodes[0].bytes[i] = 0;
                    nodes[1].bytes[i] = 0;
                }
                for(int i = 0; i < usedSize; ++i) {
                    const bool b = (rand() % 2);
                    if(b) {
                        nodes[0].flip(i);
                    }
                    bits.push_back(b);
                }
                for(int i = 0; i < move; ++i) {
                    const bool b = (rand() % 2);
                    if(b) {
                        nodes[1].flip(i);
                    }
                    bits.push_back(b);
                }
                nodes[0].moveRight(&nodes[1], usedSize, invMove);
                int j = 0;
                for(; j < move; ++j) {
                    ASSERT_E(nodes[0].access(j), bits[j])
                }
                for(; j < move + usedSize; ++j) {
                    ASSERT_E(nodes[1].access(j - move), bits[j])
                }
                //nodes[1].moveLeft(&nodes[0], usedSize, invMove);
                nodes[1].moveLeft(&nodes[0], move, invMove);
                for(j = 0; j < usedSize; ++j) {
                    ASSERT_E(nodes[0].access(j), bits[j])
                }
                for(; j < (move) + usedSize; ++j) {
                    ASSERT_E(nodes[1].access(j - usedSize), bits[j])
                }
            }
        }
    }

};

template<WordSizes wordSize, bool useBinarySearch>
class NODE2 {
public:
    constexpr static const int numChildren = 15;
    constexpr static const int maxNums = useBinarySearch ? (1 << 13) - 1 : (1 << 14) - 1;
private:
    // layout:
    // maximaler Grad 15 -> 4 bit pro index
    // 58bit childrenPtr;6bit free;3 bit free;13 bit _nums[0],...idx bit free;16-ctz(i) bit _nums[i],... selbes mit _nums

    constexpr const static std::array<uint8_t, 14> ctzLookup = {0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1};
    constexpr const static std::array<uint64_t, 8> bitMasks = []()constexpr -> std::array<uint64_t, 8> {
        if constexpr(useBinarySearch) {
            //c=1100,e=1110,f=1111
#if __BYTE_ORDER == __LITTLE_ENDIAN
            return {0x3f, 0x8000'e000'c000'e000, 0x0000'e000'c000'e000, 0x8000'e000'c000'e000, 0xc000'e000'c000'e000, 0xc000'e000'8000'e000, 0xc000'e000'0000'e000, 0xc000'e000'8000'e000};
#else
            return {0x3f, 0xe000'c000'e000'8000, 0xe000'c000'e000'0000, 0xe000'c000'e000'8000, 0xe000'c000'e000'c000, 0xe000'8000'e000'c000, 0xe000'0000'e000'c000, 0xe000'8000'e000'c000};
#endif
        } else {
            return {0x3f, 0xc000'c000'c000'c000, 0xc000'c000'c000'c000, 0xc000'c000'c000'c000, 0xc000'c000'c000'c000, 0xc000'c000'c000'c000, 0xc000'c000'c000'c000, 0xc000'c000'c000'c000};
        }
    }();
    union {
        struct {
            uint8_t *childrenPtr;
            uint16_t nums[numChildren - 1];
            uint16_t ones[numChildren - 1];
        } values;
        uint64_t rawBytes[8];
    } data;

    template<bool sizeOnly>
    inline constexpr bool isMetaCleared() const {
        if(!sizeOnly) {
            for(int i = 1; i < 8; ++i) {
                if((data.rawBytes[i] & bitMasks[i]) != 0)return false;
            }
            if constexpr(!useBinarySearch) {
                if((data.rawBytes[0] & 0xf) != 0)return false;
            }
            return true;
        } else {
            if constexpr(useBinarySearch) {
                return (data.rawBytes[0] & bitMasks[0]) == 0;
            } else {
                return (data.rawBytes[0] & 0x30) == 0;
            }
        }
    }

    template<bool sizeOnly>
    inline constexpr void writeMetaData(uint64_t metaData) {
        ASSERT_B(isMetaCleared<sizeOnly>())
        if constexpr(useBinarySearch) {
            if constexpr(!sizeOnly) {
                data.rawBytes[1] |= __builtin_ia32_pdep_di(metaData >> 53, bitMasks[1]);
                data.rawBytes[2] |= __builtin_ia32_pdep_di(metaData >> 45, bitMasks[2]);
                data.rawBytes[3] |= __builtin_ia32_pdep_di(metaData >> 36, bitMasks[3]);
                data.rawBytes[4] |= __builtin_ia32_pdep_di(metaData >> 26, bitMasks[4]);
                data.rawBytes[5] |= __builtin_ia32_pdep_di(metaData >> 17, bitMasks[5]);
                data.rawBytes[6] |= __builtin_ia32_pdep_di(metaData >> 9, bitMasks[6]);
                data.rawBytes[7] |= __builtin_ia32_pdep_di(metaData, bitMasks[7]);
            } else {
                ASSERT_E(__builtin_ia32_pext_di(data.rawBytes[0], bitMasks[0]) & 0b11, 0)
                data.rawBytes[0] |= __builtin_ia32_pdep_di(metaData >> 60, bitMasks[0] & 0b111100);
                ASSERT_E(__builtin_ia32_pext_di(data.rawBytes[0], bitMasks[0]) & 0b11, 0)
            }
        } else {
            if constexpr(!sizeOnly) {
                data.rawBytes[0] |= __builtin_ia32_pdep_di(metaData >> 56, bitMasks[0] & 0xf);
                data.rawBytes[1] |= __builtin_ia32_pdep_di(metaData >> 48, bitMasks[1]);
                data.rawBytes[2] |= __builtin_ia32_pdep_di(metaData >> 40, bitMasks[2]);
                data.rawBytes[3] |= __builtin_ia32_pdep_di(metaData >> 32, bitMasks[3]);
                data.rawBytes[4] |= __builtin_ia32_pdep_di(metaData >> 24, bitMasks[4]);
                data.rawBytes[5] |= __builtin_ia32_pdep_di(metaData >> 16, bitMasks[5]);
                data.rawBytes[6] |= __builtin_ia32_pdep_di(metaData >> 8, bitMasks[6]);
                data.rawBytes[7] |= __builtin_ia32_pdep_di(metaData, bitMasks[7]);
            } else {
                data.rawBytes[0] |= __builtin_ia32_pdep_di(metaData >> 62, bitMasks[0] & 0b110000);
                //                ASSERT_E(__builtin_ia32_pext_di(data.rawBytes[0], bitMasks[0]) & 0b1111, 0)
            }
        }
        ASSERT_E(readMetaData<sizeOnly>(), metaData)
    }


    template<bool sizeOnly>
    inline constexpr uint64_t readMetaData() const {
        if(useBinarySearch) {
            if constexpr(!sizeOnly) {
                ASSERT_L((__builtin_ia32_pext_di(data.rawBytes[1], bitMasks[1])), 1 << 7)
                //TODO: größe auch ausgeben, falls sizeOnly = false?
                return (__builtin_ia32_pext_di(data.rawBytes[1], bitMasks[1]) << 53) |//53+9 (nur untere 7 bits)
                       (__builtin_ia32_pext_di(data.rawBytes[2], bitMasks[2]) << 45) |//45+8
                       (__builtin_ia32_pext_di(data.rawBytes[3], bitMasks[3]) << 36) |//36+9
                       (__builtin_ia32_pext_di(data.rawBytes[4], bitMasks[4]) << 26) |//26+10
                       (__builtin_ia32_pext_di(data.rawBytes[5], bitMasks[5]) << 17) |//17+9
                       (__builtin_ia32_pext_di(data.rawBytes[6], bitMasks[6]) << 9) |//9+8
                       (__builtin_ia32_pext_di(data.rawBytes[7], bitMasks[7]));//0+9
            } else {
                ASSERT_E(__builtin_ia32_pext_di(data.rawBytes[0], bitMasks[0]) & 0b11, 0)
                return (__builtin_ia32_pext_di(data.rawBytes[0], bitMasks[0])) << 58;
            }
        } else {
            if constexpr(!sizeOnly) {
                return (__builtin_ia32_pext_di(data.rawBytes[0], bitMasks[0] & 0xf) << 56) |//56+6
                       (__builtin_ia32_pext_di(data.rawBytes[1], bitMasks[1]) << 48) |//48+8
                       (__builtin_ia32_pext_di(data.rawBytes[2], bitMasks[2]) << 40) |//40+8
                       (__builtin_ia32_pext_di(data.rawBytes[3], bitMasks[3]) << 32) |//32+8
                       (__builtin_ia32_pext_di(data.rawBytes[4], bitMasks[4]) << 24) |//24+8
                       (__builtin_ia32_pext_di(data.rawBytes[5], bitMasks[5]) << 16) |//16+8
                       (__builtin_ia32_pext_di(data.rawBytes[6], bitMasks[6]) << 8) |//8+8
                       (__builtin_ia32_pext_di(data.rawBytes[7], bitMasks[7]));//0+8
            } else {
                return (__builtin_ia32_pext_di(data.rawBytes[0], bitMasks[0]) & 0x30) << 58;
            }
        }
    }

    static inline constexpr uint_fast8_t getIdx(uint64_t metaData, uint_fast8_t idx) {
        ASSERT_R(scu32(idx), 0, numChildren)
        return (metaData >> (idx * 4)) & 0xf;
    }

    static inline constexpr uint64_t rotateRight(uint64_t metaData, uint_fast8_t amount) {
        ASSERT_R(amount, 0, numChildren)
        //60 bit rotation, rotiert in wirklichkeit (kontraintuitiv aber sinnvoll) nach links um 4*amount
        return __builtin_ia32_pext_di(__rolq(metaData << 4, amount * 4), __rolq(0xffff'ffff'ffff'fff0, amount * 4));
    }

    static inline constexpr uint64_t rotateLeft(uint64_t metaData, uint_fast8_t amount) {
        ASSERT_R(amount, 0, numChildren)
        //60 bit rotation, rotiert in wirklichkeit (kontraintuitiv aber sinnvoll) nach rechts um 4*amount
        return __builtin_ia32_pext_di(__rorq(metaData, amount * 4), __rorq(0x0fff'ffff'ffff'ffff, amount * 4));

    }

    static inline constexpr uint64_t pInsert(uint64_t metaData, uint_fast8_t idx) {
        ASSERT_R(idx, 0, numChildren)
        //rotiert die bits 50 bis 4*idx nach links.
        return (metaData & ((scu64(1) << (4 * idx)) - 1)) | (__builtin_ia32_pext_di(__rolq(metaData << 4, 4), __rolq(0xffff'ffff'ffff'fff0 << (4 * idx), 4)) << 4 * idx);
    }

    static inline constexpr uint64_t pRemove(uint64_t metaData, uint_fast8_t idx) {
        ASSERT_R(idx, 0, numChildren)
        //rotiert die bits 50 bis 4*idx nach rechts.
        return (metaData & ((scu64(1) << (4 * idx)) - 1)) | (__builtin_ia32_pext_di(__rorq(metaData >> (4 * idx), 4), __rorq(0x0fff'ffff'ffff'ffff >> (4 * idx), 4)) << (4 * idx));
    }

    inline void normalize(uint_fast8_t _size) {
        ASSERT_B(isMetaCleared<true>() && isMetaCleared<false>())
        if constexpr(useBinarySearch) {
            _size -= (_size == numChildren);
            uint16_t numsArr[4] = {}, onesArr[4] = {};
            for(int i = 0; i < _size; ++i) {
                const uint_fast8_t idx = ctzLookup[i];
                numsArr[idx] = data.values.nums[i];
                onesArr[idx] = data.values.ones[i];
                switch(idx) {
                    case 3:
                        data.values.nums[i] -= numsArr[2];
                        data.values.ones[i] -= onesArr[2];
                        [[fallthrough]];
                    case 2:
                        data.values.nums[i] -= numsArr[1];
                        data.values.ones[i] -= onesArr[1]; [[fallthrough]];
                    case 1:
                        data.values.nums[i] -= numsArr[0];
                        data.values.ones[i] -= onesArr[0]; [[fallthrough]];
                    default:
                        (void) 0;
                }
                ASSERT_GE(__builtin_clz(data.values.nums[i]) - 16, 3)
                ASSERT_GE(__builtin_clz(data.values.ones[i]) - 16, 3)
            }
        } else return;
    }

    inline void denormalize(uint_fast8_t _size) {
        ASSERT_B(isMetaCleared<true>() && isMetaCleared<false>())
        if constexpr(useBinarySearch) {
            _size -= (_size == numChildren);
            uint16_t numsArr[4], onesArr[4];
            for(int i = 0; i < _size; ++i) {
                const uint_fast8_t idx = ctzLookup[i];
                numsArr[idx] = data.values.nums[i];
                onesArr[idx] = data.values.ones[i];
                switch(idx) {
                    case 3:
                        numsArr[idx] += numsArr[2];
                        onesArr[idx] += onesArr[2];[[fallthrough]];
                    case 2:
                        numsArr[idx] += numsArr[1];
                        onesArr[idx] += onesArr[1];[[fallthrough]];
                    case 1:
                        numsArr[idx] += numsArr[0];
                        onesArr[idx] += onesArr[0];[[fallthrough]];
                    default:
                        (void) 0;
                }
                data.values.ones[i] = onesArr[idx];
                data.values.nums[i] = numsArr[idx];
                ASSERT_GE(__builtin_clz(data.values.nums[idx]) - 16, 3 - idx)
                ASSERT_GE(__builtin_clz(data.values.ones[idx]) - 16, 3 - idx)
            }
        } else return;
    }

    template<bool nums>
    inline uint32_t getSumExceptLast() const {
        if constexpr(useBinarySearch) {
            ASSERT_B(isMetaCleared<true>() || isMetaCleared<false>())
            if constexpr(nums) {
                return data.values.nums[7] + data.values.nums[11] + data.values.nums[13];
            } else {
                return data.values.ones[7] + data.values.ones[11] + data.values.ones[13];
            }
        } else {
            ASSERT_UNIMPLEMETED()
            return 0;
        }
    }

public:
    inline constexpr void clearMetadata() {
        for(int i = 0; i < 8; ++i) {
            data.rawBytes[i] &= ~bitMasks[i];
        }
    }

    template<class intType, bool isLowest>
    inline void moveLeft(NODE2 *lNode, const uint_fast8_t delta, const uint_fast8_t lSize, const uint_fast8_t rSize, intType &lNums, intType &rNums, intType &lOnes, intType &rOnes) {
        static_assert(is_integral_v<intType>);
        static_assert(!is_signed_v<intType>);
        using T = std::conditional_t<isLowest, NODE0, NODE2 >;
        //   ASSERT_GE(delta, 1)
        // ASSERT_B(!isMetaCleared<true>() && !isMetaCleared<false>() && !lNode->isMetaCleared<true>() && !lNode->isMetaCleared<false>())
        const uint64_t lmeta = lNode->template readMetaData<false>(), rmeta = readMetaData<false>();
        ASSERT_LE(lSize + delta, numChildren)
        //ASSERT_GE(rSize - delta, numChildren / 2)
        ASSERT_NE(lNode, this)
        lNode->clearMetadata(), clearMetadata();
        lNode->normalize(lSize), normalize(rSize);
        for(int i = 0; i < delta - 1; ++i) {
            const uint16_t curNums = data.values.nums[i], curOnes = data.values.ones[i];
            lNode->data.values.nums[lSize + i] = curNums;
            lNode->data.values.ones[lSize + i] = curOnes;
            lNums += curNums, lOnes += curOnes;
            rNums -= curNums, rOnes -= curOnes;
            std::memcpy(lNode->getChildrenPtr() + (getIdx(lmeta, lSize + i) * sizeof(T)), getChildrenPtr() + (getIdx(rmeta, i) * sizeof(T)), sizeof(T));
        }
        if(lSize + delta == numChildren) {
            const uint16_t curNums = data.values.nums[delta - 1], curOnes = data.values.ones[delta - 1];
            lNums += curNums, lOnes += curOnes;
            rNums -= curNums, rOnes -= curOnes;
            std::memcpy(lNode->getChildrenPtr() + (getIdx(lmeta, lSize + delta - 1) * sizeof(T)), getChildrenPtr() + (getIdx(rmeta, delta - 1) * sizeof(T)), sizeof(T));
        } else {
            for(int i = lSize + delta; i < numChildren - 1; ++i) {
                lNode->data.values.nums[i] = 0xffff;
            }
            const uint16_t curNums = data.values.nums[delta - 1], curOnes = data.values.ones[delta - 1];
            lNode->data.values.nums[lSize + delta - 1] = curNums;
            lNode->data.values.ones[lSize + delta - 1] = curOnes;
            lNums += curNums, lOnes += curOnes;
            rNums -= curNums, rOnes -= curOnes;
            std::memcpy(lNode->getChildrenPtr() + (getIdx(lmeta, lSize + delta - 1) * sizeof(T)), getChildrenPtr() + (getIdx(rmeta, delta - 1) * sizeof(T)), sizeof(T));

        }
        intType copyFullNums = rNums, copyFullOnes = rOnes;
        for(int i = 0; i < rSize - delta - 1; ++i) {
            const uint16_t curNums = data.values.nums[i + delta], curOnes = data.values.ones[i + delta];
            data.values.nums[i] = curNums;
            data.values.ones[i] = curOnes;
            data.values.nums[i + delta] = 0xffff;
            data.values.ones[i + delta] = 0xffff;
            copyFullNums -= curNums;
            copyFullOnes -= curOnes;
        }
        if(rSize != numChildren) {
            data.values.nums[rSize - delta - 1] = data.values.nums[rSize - 1];
            data.values.ones[rSize - delta - 1] = data.values.ones[rSize - 1];
        } else {
            data.values.nums[rSize - delta - 1] = copyFullNums;
            data.values.ones[rSize - delta - 1] = copyFullOnes;
        }
        for(int i = rSize - delta; i < numChildren - 1; ++i) {
            data.values.nums[i] = 0xffff;
            data.values.ones[i] = 0xffff;
        }
        ASSERT_CODE_LOCALE(if constexpr(isLowest) {
            for(int i = 0; i < numChildren - 1; ++i) {
                ASSERT_PR(data.values.nums[i] != 0xffff, data.values.nums[i], wordSize * wordSize / 2, wordSize * wordSize + 1)
            }
        })
        lNode->clearMetadata(), clearMetadata();
        lNode->denormalize(lSize + delta), denormalize(rSize - delta);
        lNode->writeMetaData<false>(lmeta), writeMetaData<false>(rotateLeft(rmeta, delta));
        lNode->setSize(lSize + delta), setSize(rSize - delta);
    }

    template<class intType, bool isLowest>
    inline void moveRight(NODE2 *rNode, const uint_fast8_t delta, const uint_fast8_t lSize, const uint_fast8_t rSize, intType &lNums, intType &rNums, intType &lOnes, intType &rOnes) {
        static_assert(is_integral_v<intType>);
        static_assert(!is_signed_v<intType>);
        using T = std::conditional_t<isLowest, NODE0, NODE2 >;
        // ASSERT_B(!isMetaCleared<true>() && !isMetaCleared<false>() && !rNode->isMetaCleared<true>() && !rNode->isMetaCleared<false>())
        const uint64_t lmeta = readMetaData<false>(), rmeta = rotateRight(rNode->template readMetaData<false>(), delta);
        ASSERT_LE(rSize + delta, numChildren)
        ASSERT_GE(lSize - delta, numChildren / 2)
        ASSERT_NE(rNode, this)
        rNode->clearMetadata(), clearMetadata();
        rNode->normalize(rSize), normalize(lSize);
        for(int i = numChildren - 2; i >= delta; --i) {
            rNode->data.values.nums[i] = rNode->data.values.nums[i - delta];
            rNode->data.values.ones[i] = rNode->data.values.ones[i - delta];
        }
        for(int i = rSize + delta; i < numChildren - 1; ++i) {
            rNode->data.values.nums[i] = 0xffff;
            rNode->data.values.ones[i] = 0xffff;
        }
        for(int i = 0; i < delta - (lSize == numChildren); ++i) {
            const uint16_t curNums = data.values.nums[i + lSize - delta], curOnes = data.values.ones[i + lSize - delta];
            data.values.nums[i + lSize - delta] = 0xffff;
            data.values.ones[i + lSize - delta] = 0xffff;
            rNode->data.values.nums[i] = curNums;
            rNode->data.values.ones[i] = curOnes;
            lNums -= curNums, lOnes -= curOnes;
            rNums += curNums, rOnes += curOnes;
            std::memcpy(rNode->getChildrenPtr() + (getIdx(rmeta, i) * sizeof(T)), getChildrenPtr() + (getIdx(lmeta, i + lSize - delta) * sizeof(T)), sizeof(T));
        }
        if(lSize == numChildren) {
            intType copyFullNums = lNums, copyFullOnes = lOnes;
            for(int i = 0; i < lSize - delta; ++i) {
                copyFullNums -= data.values.nums[i];
                copyFullOnes -= data.values.ones[i];
            }
            rNode->data.values.nums[delta - 1] = copyFullNums;
            rNode->data.values.ones[delta - 1] = copyFullOnes;
            lNums -= copyFullNums;
            lOnes -= copyFullOnes;
            rNums += copyFullNums;
            rOnes += copyFullOnes;
        }
        std::memcpy(rNode->getChildrenPtr() + (getIdx(rmeta, delta - 1) * sizeof(T)), getChildrenPtr() + (getIdx(lmeta, lSize - 1) * sizeof(T)), sizeof(T));
        ASSERT_CODE_LOCALE(if constexpr(isLowest) {
            for(int i = 0; i < numChildren - 1; ++i) {
                ASSERT_PR(data.values.nums[i] != 0xffff, data.values.nums[i], wordSize * wordSize / 2, wordSize * wordSize + 1)
            }
            for(int i = 0; i < rSize + delta && i < numChildren - 1; ++i) {
                ASSERT_R(RCNODE_0(rNode->getChildrenPtr())[getIdx(rmeta, i)].template rank<true>(rNode->data.values.nums[i] - (rNode->data.values.nums[i] == wordSize * wordSize)), rNode->data.values.ones[i] - 1, rNode->data.values.ones[i] + 1, i)
            }
            for(int i = 0; i < lSize - delta && i < numChildren - 1; ++i) {
                ASSERT_R(RCNODE_0(getChildrenPtr())[getIdx(lmeta, i)].template rank<true>(data.values.nums[i] - (data.values.nums[i] == wordSize * wordSize)), data.values.ones[i] - 1, data.values.ones[i] + 1, i)
            }
        } else {
            for(int i = 0; i < delta; ++i) {
                uint64_t *rval = rc<uint64_t *>(rNode->getChildrenPtr()) + (8 * getIdx(rmeta, i));
                uint64_t *lval = rc<uint64_t *>(getChildrenPtr()) + (8 * getIdx(lmeta, i + lSize - delta));
                for(int j = 0; j < 8; ++j) {
                    ASSERT_E(rval[j], lval[j])
                }
            }
        })
        clearMetadata(), rNode->clearMetadata();
        denormalize(lSize - delta), rNode->denormalize(rSize + delta);
        writeMetaData<false>(lmeta), rNode->writeMetaData<false>(rmeta);
        setSize(lSize - delta), rNode->setSize(rSize + delta);
    }

    inline uint_fast8_t toRealIdx(uint_fast8_t virtualIndex) const {
        return getIdx(readMetaData<false>(), virtualIndex);
    }

    inline uint_fast8_t toRealIdx(uint64_t meta, uint_fast8_t virtualIndex) const {
        return getIdx(meta, virtualIndex);
    }

    inline uint64_t remove(uint_fast8_t idx, uint16_t lastNums, uint16_t lastOnes, uint64_t meta) {
        for(int i = idx; i < numChildren - 1; ++i) {
            data.values.nums[i] = data.values.nums[i + 1];
            data.values.ones[i] = data.values.ones[i + 1];
        }
        data.values.nums[numChildren - 1] = lastNums;
        data.values.ones[numChildren - 1] = lastOnes;
        return pRemove(meta, idx);
    }

    template<bool tryAlloc>
    inline std::conditional_t<tryAlloc, bool, void> lowestAllocate(uint64_t fullNums, uint64_t curNums, uint8_t *&lowerChild, uint64_t &key, uint_fast8_t &idx) {
        ASSERT_R(scu32(size()), 2, numChildren + 1)
        ASSERT_R(scu32(idx), 0, scu32(size()))
        ASSERT_CODE_LOCALE(
                uint_fast8_t assert_size = size();
                uint64_t assert_meta = readMetaData<false>();
                clearMetadata();
                normalize(assert_size);
                ASSERT_PE(assert_size != numChildren, curNums, data.values.nums[idx])
                denormalize(assert_size);
                writeMetaData<false>(assert_meta);
                setSize(assert_size);
        )
        if constexpr(tryAlloc) {
            if(curNums < wordSize * wordSize)
                return true;
        } else { ASSERT_E(curNums, wordSize * wordSize) }
        uint_fast8_t _size = size();
        const auto allocate = [this](bool lowest, bool highest, uint_fast8_t &_size, uint64_t &meta, uint8_t *&lowerChild, uint64_t &key, uint_fast8_t &idx) {
            ASSERT_R(idx, 1, numChildren - 2)
            ASSERT_B(!lowest || !highest)
            ASSERT_E(data.values.nums[idx - 1], wordSize * wordSize)
            ASSERT_E(data.values.nums[idx], wordSize * wordSize)
            ASSERT_E(data.values.nums[idx + 1], wordSize * wordSize)
            for(int i = _size - (_size == numChildren - 1); i > idx; --i) {
                data.values.nums[i] = data.values.nums[i - 1];
                data.values.ones[i] = data.values.ones[i - 1];
            }
            const uint16_t ones2 = RCNODE_0(getChildrenPtr())[getIdx(meta, idx)].template rank<true>((wordSize * wordSize) / 2), ones3 = RCNODE_0(getChildrenPtr())[getIdx(meta, idx + 1)].template rank<true>((wordSize * wordSize) / 4);
            meta = pInsert(meta, idx);
            RCNODE_0(getChildrenPtr())[getIdx(meta, idx - 1)].moveRight(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx)], wordSize * wordSize, (wordSize * wordSize) / 4);
            RCNODE_0(getChildrenPtr())[getIdx(meta, idx + 1)].moveLeft(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx)], (wordSize * wordSize) / 4, (wordSize * wordSize) / 2);
            RCNODE_0(getChildrenPtr())[getIdx(meta, idx + 2)].moveLeft(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx + 1)], (wordSize * wordSize) / 2, (wordSize * wordSize) / 4);
            const uint16_t ones1 = RCNODE_0(getChildrenPtr())[getIdx(meta, idx)].template rank<true>((wordSize * wordSize) / 4);
            data.values.nums[idx - 1] = (wordSize * wordSize * 3) / 4;
            data.values.nums[idx] = (wordSize * wordSize * 3) / 4;
            data.values.nums[idx + 1] = (wordSize * wordSize * 3) / 4;
            if(idx != numChildren - 3) data.values.nums[idx + 2] = (wordSize * wordSize * 3) / 4;
            data.values.ones[idx - 1] -= ones1;
            data.values.ones[idx] = ones1 + ones2;
            data.values.ones[idx + 1] = data.values.ones[idx + 1] - ones2 + ones3;
            if(idx != numChildren - 3) data.values.ones[idx + 2] -= ones3;
            if(lowest) {
                if(key > (wordSize * wordSize * 3) / 4) {
                    key -= (wordSize * wordSize * 3) / 4;
                    lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, 1)]);
                    idx = 1;
                } else {
                    idx = 0;
                }
            } else if(highest) {
                if(key < (wordSize * wordSize) / 4) {
                    key += (wordSize * wordSize) / 2;
                    ++idx;
                    lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx)]);
                } else {
                    key -= (wordSize * wordSize) / 4;
                    idx += 2;
                    ASSERT_E(rcuint(lowerChild), rcuint(rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx)])))
                    //lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx)]);
                }
            } else {
                if(key < (wordSize * wordSize) / 2) {
                    key += (wordSize * wordSize) / 4;
                    lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx)]);
                } else {
                    key -= (wordSize * wordSize) / 2;
                    ++idx;
                    ASSERT_E(rcuint(lowerChild), rcuint(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx)]), rcuint(getChildrenPtr()))
                }
            }
            ++_size;
        };
        uint64_t meta = readMetaData<false>();
        bool out = true;
        clearMetadata();
        uint16_t lastNums = fullNums - getSumExceptLast<true>();
        ASSERT_PR(_size == numChildren, lastNums, (wordSize * wordSize) / 2, (wordSize * wordSize) + 1)
        normalize(_size);
        ASSERT_PE(_size != numChildren, curNums, data.values.nums[idx])
        ASSERT_LE(data.values.nums[idx], wordSize * wordSize)
        ASSERT_PE(idx < numChildren - 1, data.values.nums[idx], wordSize * wordSize)
        ASSERT_PE(idx == numChildren - 1, curNums, wordSize * wordSize)
        if(RANGE(idx, 1, _size - 1)) {
            ASSERT_E(data.values.nums[idx], wordSize * wordSize)
            if(idx != numChildren - 2) {
                if((scu32(data.values.nums[idx - 1]) + scu32(data.values.nums[idx]) + scu32(data.values.nums[idx + 1])) < (wordSize * wordSize * 3)) {
                    if(scu32(data.values.nums[idx - 1]) < (scu32(data.values.nums[idx + 1]))) {
                        if(key == 0) {
                            --idx;
                            key = data.values.nums[idx];
                            lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx)]);
                        } else {
                            uint16_t delta = DIV_ROUNDUP(((wordSize * wordSize) - data.values.nums[idx - 1]), 2);
                            ASSERT_R(delta, 1, ((wordSize * wordSize) / 4) + 1)
                            ASSERT_LE(data.values.nums[idx - 1] + delta, wordSize * wordSize)
                            uint16_t ones = RCNODE_0(getChildrenPtr())[getIdx(meta, idx)].template rank<true>(delta);
                            RCNODE_0(getChildrenPtr())[getIdx(meta, idx)].moveLeft(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx - 1)], data.values.nums[idx - 1], delta);
                            if(key < delta) {
                                key += data.values.nums[idx - 1];
                                data.values.nums[idx] -= delta;
                                data.values.nums[idx - 1] += delta;
                                data.values.ones[idx] -= ones;
                                data.values.ones[idx - 1] += ones;
                                --idx;
                                lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx)]);
                            } else {
                                data.values.nums[idx] -= delta;
                                data.values.nums[idx - 1] += delta;
                                data.values.ones[idx] -= ones;
                                data.values.ones[idx - 1] += ones;
                                key -= delta;
                            }
                        }
                    } else {
                        if(key == wordSize * wordSize) {
                            key = 0;
                            ++idx;
                            lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx)]);
                        } else {
                            uint16_t delta = DIV_ROUNDUP(((wordSize * wordSize) - data.values.nums[idx + 1]), 2);
                            ASSERT_R(delta, 1, ((wordSize * wordSize) / 4) + 1)
                            ASSERT_LE(data.values.nums[idx + 1] + delta, wordSize * wordSize)
                            RCNODE_0(getChildrenPtr())[getIdx(meta, idx)].moveRight(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx + 1)], data.values.nums[idx], delta);
                            uint16_t ones = RCNODE_0(getChildrenPtr())[getIdx(meta, idx + 1)].template rank<true>(delta);
                            data.values.nums[idx] -= delta;
                            data.values.nums[idx + 1] += delta;
                            data.values.ones[idx] -= ones;
                            data.values.ones[idx + 1] += ones;
                            if(key > data.values.nums[idx]) {
                                key -= data.values.nums[idx];
                                ++idx;
                                lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx)]);
                            }
                        }
                    }
                } else {
                    ASSERT_E(scu32(data.values.nums[idx - 1]) + scu32(data.values.nums[idx]) + scu32(data.values.nums[idx + 1]), (wordSize * wordSize * 3))
                    if(_size == numChildren) {
                        if constexpr(tryAlloc) out = false; else { ASSERT_UNREACHEABLE() }
                    } else {
                        allocate(false, false, _size, meta, lowerChild, key, idx);
                    }
                }
            } else {
                if((scu32(data.values.nums[numChildren - 3]) + scu32(data.values.nums[numChildren - 2]) + lastNums) < (wordSize * wordSize * 3)) {
                    if(scu32(data.values.nums[numChildren - 3]) < scu32(lastNums)) {
                        if(key == 0) {
                            idx = numChildren - 3;
                            lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 3)]);
                            key = scu32(data.values.nums[numChildren - 3]);
                        } else {
                            uint16_t delta = DIV_ROUNDUP(((wordSize * wordSize) - data.values.nums[numChildren - 3]), 2);
                            ASSERT_R(delta, 1, ((wordSize * wordSize) / 4) + 1)
                            ASSERT_LE(data.values.nums[numChildren - 3] + delta, wordSize * wordSize)
                            uint16_t ones = RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 2)].template rank<true>(delta);
                            RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 2)].moveLeft(&RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 3)], data.values.nums[numChildren - 3], delta);
                            if(key < delta) {
                                idx = numChildren - 3;
                                key += data.values.nums[numChildren - 3];
                                lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 3)]);
                            } else {
                                key -= delta;
                            }
                            data.values.nums[numChildren - 2] -= delta;
                            data.values.nums[numChildren - 3] += delta;
                            data.values.ones[numChildren - 2] -= ones;
                            data.values.ones[numChildren - 3] += ones;
                        }
                    } else {
                        if(key == wordSize * wordSize) {
                            key = 0;
                            idx = numChildren - 1;
                            lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 1)]);
                        } else {
                            uint16_t delta = DIV_ROUNDUP(((wordSize * wordSize) - lastNums), 2);
                            ASSERT_R(delta, 0, ((wordSize * wordSize) / 4) + 1)
                            ASSERT_LE(lastNums + delta, wordSize * wordSize)
                            RCNODE_0(getChildrenPtr())[getIdx(meta, 13)].moveRight(&RCNODE_0(getChildrenPtr())[getIdx(meta, 14)], data.values.nums[13], delta);
                            uint16_t ones = RCNODE_0(getChildrenPtr())[getIdx(meta, 14)].template rank<true>(delta);
                            data.values.nums[numChildren - 2] -= delta;
                            data.values.ones[numChildren - 2] -= ones;
                            if(key > data.values.nums[numChildren - 2]) {
                                key -= data.values.nums[numChildren - 2];
                                idx = numChildren - 1;
                                lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 1)]);
                            }
                        }
                    }
                } else {
                    ASSERT_E(scu32(data.values.nums[idx - 1]) + scu32(data.values.nums[idx]) + lastNums, (wordSize * wordSize * 3))
                    ASSERT_E(_size, numChildren)
                    if constexpr(tryAlloc) out = false; else { ASSERT_UNREACHEABLE() }

                }
            }
        } else {
            ASSERT_PE(idx < numChildren - 1, data.values.nums[idx], wordSize * wordSize)
            ASSERT_PE(idx == numChildren - 1, lastNums, wordSize * wordSize)
            switch(idx) {
                case 0: {
                    if((scu32(data.values.nums[0]) + scu32(data.values.nums[1]) + scu32((_size > 2) * data.values.nums[2])) < (wordSize * wordSize * (2 + (_size > 2)))) {
                        if(data.values.nums[1] < scu32(wordSize * wordSize)) {
                            if(key == wordSize * wordSize) {
                                key = 0;
                                idx = 1;
                                lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, 1)]);
                            } else {
                                uint16_t delta = DIV_ROUNDUP((wordSize * wordSize) - data.values.nums[1], 2);
                                ASSERT_R(delta, 1, ((wordSize * wordSize) / 4) + 1)
                                ASSERT_LE(data.values.nums[1] + delta, wordSize * wordSize)
                                RCNODE_0(getChildrenPtr())[getIdx(meta, 0)].moveRight(&RCNODE_0(getChildrenPtr())[getIdx(meta, 1)], data.values.nums[0], delta);
                                uint16_t ones = RCNODE_0(getChildrenPtr())[getIdx(meta, 1)].template rank<true>(delta);
                                data.values.nums[0] -= delta;
                                data.values.nums[1] += delta;
                                data.values.ones[0] -= ones;
                                data.values.ones[1] += ones;
                                if(key > wordSize * wordSize - delta) {
                                    idx = 1;
                                    lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, 1)]);
                                    key -= wordSize * wordSize - delta;
                                }
                            }
                        } else {
                            //uint16_t delta = DIV_ROUNDUP((wordSize * wordSize) - data.values.nums[2], 2);
                            ((wordSize * wordSize) + (wordSize * wordSize) + data.values.nums[2]) / 3;
                            uint16_t delta1 = DIV_ROUNDUP((wordSize * wordSize) - data.values.nums[2], 3), delta2 = ((wordSize * wordSize) - data.values.nums[2]) / 3;
                            if(key > wordSize * wordSize - delta1) {
                                swap(delta1, delta2);
                                idx = 1;
                                lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, 1)]);
                                key -= wordSize * wordSize - delta1;
                            }
                            ASSERT_R(delta1, 0, ((wordSize * wordSize) / 4) + 1)
                            ASSERT_R(delta2, 0, ((wordSize * wordSize) / 4) + 1)
                            ASSERT_LE(data.values.nums[2] + delta1 + delta2, wordSize * wordSize)
                            RCNODE_0(getChildrenPtr())[getIdx(meta, 1)].moveRight(&RCNODE_0(getChildrenPtr())[getIdx(meta, 2)], wordSize * wordSize, delta1 + delta2);
                            if(delta1 != 0) RCNODE_0(getChildrenPtr())[getIdx(meta, 0)].moveRight(&RCNODE_0(getChildrenPtr())[getIdx(meta, 1)], wordSize * wordSize, delta1);
                            uint16_t ones1 = RCNODE_0(getChildrenPtr())[getIdx(meta, 1)].template rank<true>(delta1), ones2 = RCNODE_0(getChildrenPtr())[getIdx(meta, 2)].template rank<true>(delta1 + delta2);
                            data.values.nums[0] -= delta1;
                            data.values.nums[1] -= delta2;
                            data.values.nums[2] += delta1 + delta2;
                            data.values.ones[0] -= ones1;
                            data.values.ones[1] = data.values.ones[1] + ones1 - ones2;
                            data.values.ones[2] += ones2;
                        }
                    } else {
                        ASSERT_E(scu32(data.values.nums[0]) + scu32(data.values.nums[1]) + (scu32((_size > 2) * data.values.nums[2])), wordSize * wordSize * (2 + (_size > 2)))
                        if(_size == numChildren) {
                            if constexpr(tryAlloc) out = false; else { ASSERT_UNREACHEABLE() }
                        } else if(_size == 2) {
                            const uint16_t delta1 = DIV_ROUNDUP((wordSize * wordSize), 3), delta2 = ((wordSize * wordSize)) / 3;
                            RCNODE_0(getChildrenPtr())[getIdx(meta, 1)].moveRight(&RCNODE_0(getChildrenPtr())[getIdx(meta, 2)], wordSize * wordSize, delta1 + delta2);
                            RCNODE_0(getChildrenPtr())[getIdx(meta, 0)].moveRight(&RCNODE_0(getChildrenPtr())[getIdx(meta, 1)], wordSize * wordSize, delta1);
                            const uint16_t ones1 = RCNODE_0(getChildrenPtr())[getIdx(meta, 1)].template rank<true>(delta1), ones2 = RCNODE_0(getChildrenPtr())[getIdx(meta, 2)].template rank<true>(delta1 + delta2);
                            data.values.nums[0] -= delta1;
                            data.values.nums[1] -= delta2;
                            data.values.nums[2] = delta1 + delta2;
                            data.values.ones[0] -= ones1;
                            data.values.ones[1] = data.values.ones[1] + ones1 - ones2;
                            data.values.ones[2] = ones2;
                            if(key > wordSize * wordSize - delta1) {
                                idx = 1;
                                lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, 1)]);
                                key -= wordSize * wordSize - delta1;
                            }
                            ++_size;
                        } else {
                            allocate(true, false, _size, meta, lowerChild, key, ++idx);
                        }
                    }
                }
                    break;
                case 1: {
                    if(scu32(data.values.nums[0]) < scu32(wordSize * wordSize)) {
                        if(key == 0) {
                            idx = 0;
                            lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, 0)]);
                            key = scu32(data.values.nums[0]);
                        } else {
                            uint16_t delta = DIV_ROUNDUP(((wordSize * wordSize) - data.values.nums[0]), 2);
                            ASSERT_R(delta, 1, ((wordSize * wordSize) / 4) + 1)
                            ASSERT_LE(data.values.nums[0] + delta, wordSize * wordSize)
                            uint16_t ones = RCNODE_0(getChildrenPtr())[getIdx(meta, 1)].template rank<true>(delta);
                            RCNODE_0(getChildrenPtr())[getIdx(meta, 1)].moveLeft(&RCNODE_0(getChildrenPtr())[getIdx(meta, 0)], data.values.nums[0], delta);
                            if(key < delta) {
                                idx = 0;
                                key += data.values.nums[0];
                                lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, 0)]);
                            } else {
                                key -= delta;
                            }
                            data.values.nums[1] -= delta;
                            data.values.nums[0] += delta;
                            data.values.ones[1] -= ones;
                            data.values.ones[0] += ones;
                        }
                    } else {
                        RCNODE_0(getChildrenPtr())[getIdx(meta, 1)].moveRight(&RCNODE_0(getChildrenPtr())[getIdx(meta, 2)], wordSize * wordSize, (wordSize * wordSize) / 2);
                        uint16_t ones = RCNODE_0(getChildrenPtr())[getIdx(meta, 2)].template rank<true>((wordSize * wordSize) / 2);
                        data.values.nums[1] = (wordSize * wordSize) / 2;
                        data.values.nums[2] = (wordSize * wordSize) / 2;
                        data.values.ones[1] -= ones;
                        data.values.ones[2] = ones;
                        if(key > (wordSize * wordSize) / 2) {
                            idx = 2;
                            lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, 2)]);
                            key -= wordSize * wordSize / 2;
                        }
                        ++_size;
                    }
                }
                    break;
                case numChildren - 1: {
                    if((scu32(data.values.nums[numChildren - 3]) + scu32(data.values.nums[numChildren - 2]) + lastNums) < (wordSize * wordSize * 3)) {
                        if(data.values.nums[numChildren - 2] < scu32(wordSize * wordSize)) {
                            if(key == 0) {
                                key = data.values.nums[numChildren - 2];
                                idx = numChildren - 2;
                                lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 2)]);
                            } else {
                                uint16_t delta = DIV_ROUNDUP((wordSize * wordSize) - data.values.nums[numChildren - 2], 2);
                                ASSERT_R(delta, 1, ((wordSize * wordSize) / 4) + 1)
                                ASSERT_LE(data.values.nums[13] + delta, wordSize * wordSize)
                                uint16_t ones = RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 1)].template rank<true>(delta);
                                RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 1)].moveLeft(&RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 2)], data.values.nums[numChildren - 2], delta);
                                if(key < delta) {
                                    idx = numChildren - 2;
                                    key += data.values.nums[numChildren - 2];
                                    lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 2)]);
                                } else {
                                    key -= delta;
                                }
                                data.values.nums[numChildren - 2] += delta;
                                data.values.ones[numChildren - 2] += ones;
                            }
                        } else {
                            // ASSERT_UNREACHEABLE("2/3 everything")
                            uint16_t delta1 = ((wordSize * wordSize) - data.values.nums[numChildren - 3]) / 3, delta2 = DIV_ROUNDUP(((wordSize * wordSize) - data.values.nums[numChildren - 3]), 3);
                            ASSERT_R(delta1, 0, ((wordSize * wordSize) / 4) + 1)
                            ASSERT_R(delta2, 1, ((wordSize * wordSize) / 4) + 1)
                            if(key < delta2) {
                                //  swap(delta1, delta2);
                                idx = numChildren - 2;
                                key += wordSize * wordSize - delta2;
                                lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 2)]);
                            } else {
                                key -= delta2;
                            }
                            ASSERT_LE(data.values.nums[numChildren - 3] + delta1 + delta2, wordSize * wordSize)
                            uint16_t ones1 = RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 2)].template rank<true>(delta1 + delta2), ones2 = RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 1)].template rank<true>(delta2);
                            RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 2)].moveLeft(&RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 3)], data.values.nums[numChildren - 3], delta1 + delta2);
                            RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 1)].moveLeft(&RCNODE_0(getChildrenPtr())[getIdx(meta, numChildren - 2)], data.values.nums[numChildren - 2] - (delta1 + delta2), delta2);
                            data.values.nums[numChildren - 3] += delta1 + delta2;
                            data.values.nums[numChildren - 2] -= delta1;
                            data.values.ones[numChildren - 3] += ones1;
                            data.values.ones[numChildren - 2] = data.values.ones[numChildren - 2] + ones2 - ones1;
                        }

                    } else {
                        if constexpr(tryAlloc) out = false; else { ASSERT_UNREACHEABLE() }
                    }
                }
                    break;
                default: {
                    if((scu32(data.values.nums[idx - 2]) + scu32(data.values.nums[idx - 1]) + scu32(data.values.nums[idx])) < (wordSize * wordSize * 3)) {
                        if(scu32(data.values.nums[idx - 1]) < scu32(wordSize * wordSize)) {
                            if(key == 0) {
                                --idx;
                                key = data.values.nums[idx];
                                lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx)]);
                            } else {
                                uint16_t delta = DIV_ROUNDUP((wordSize * wordSize) - scu32(data.values.nums[idx - 1]), 2);
                                ASSERT_R(delta, 1, ((wordSize * wordSize) / 4) + 1)
                                ASSERT_LE(scu32(data.values.nums[idx - 1]) + delta, wordSize * wordSize)
                                uint16_t ones = RCNODE_0(getChildrenPtr())[getIdx(meta, idx)].template rank<true>(delta);
                                RCNODE_0(getChildrenPtr())[getIdx(meta, idx)].moveLeft(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx - 1)], data.values.nums[idx - 1], delta);
                                if(key < delta) {
                                    key += data.values.nums[idx - 1];
                                    data.values.nums[idx - 1] += delta;
                                    data.values.ones[idx - 1] += ones;
                                    data.values.nums[idx] -= delta;
                                    data.values.ones[idx] -= ones;
                                    --idx;
                                    lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx - 1)]);
                                } else {
                                    data.values.nums[idx - 1] += delta;
                                    data.values.ones[idx - 1] += ones;
                                    data.values.nums[idx] -= delta;
                                    data.values.ones[idx] -= ones;
                                    key -= delta;
                                }
                            }
                        } else {
                            // ASSERT_UNREACHEABLE("2/3 everything")
                            //uint16_t delta = DIV_ROUNDUP((wordSize * wordSize) - data.values.nums[idx - 2], 2);
                            uint16_t delta1 = ((wordSize * wordSize) - data.values.nums[idx - 2]) / 3, delta2 = DIV_ROUNDUP(((wordSize * wordSize) - data.values.nums[idx - 2]), 3);
                            ASSERT_R(delta1, 0, ((wordSize * wordSize) / 4) + 1)
                            ASSERT_R(delta2, 1, ((wordSize * wordSize) / 4) + 1)
                            const uint_fast8_t cidx = idx;
                            if(key < delta2) {
                                // swap(delta1, delta2);
                                --idx;
                                key += wordSize * wordSize - delta2;
                                lowerChild = rcbyte(&RCNODE_0(getChildrenPtr())[getIdx(meta, idx)]);
                            } else {
                                key -= delta2;
                            }
                            ASSERT_LE(data.values.nums[cidx - 2] + delta1 + delta2, wordSize * wordSize)
                            uint16_t ones1 = RCNODE_0(getChildrenPtr())[getIdx(meta, cidx - 1)].template rank<true>(delta1 + delta2), ones2 = RCNODE_0(getChildrenPtr())[getIdx(meta, cidx)].template rank<true>(delta2);
                            RCNODE_0(getChildrenPtr())[getIdx(meta, cidx - 1)].moveLeft(&RCNODE_0(getChildrenPtr())[getIdx(meta, cidx - 2)], data.values.nums[cidx - 2], delta1 + delta2);
                            RCNODE_0(getChildrenPtr())[getIdx(meta, cidx)].moveLeft(&RCNODE_0(getChildrenPtr())[getIdx(meta, cidx - 1)], data.values.nums[cidx - 1] - (delta1 + delta2), delta2);
                            data.values.nums[cidx - 2] += delta1 + delta2;
                            data.values.nums[cidx - 1] -= delta1;
                            data.values.nums[cidx] -= delta2;
                            data.values.ones[cidx - 2] += ones1;
                            data.values.ones[cidx - 1] = data.values.ones[cidx - 1] + ones2 - ones1;
                            data.values.ones[cidx] -= ones2;
                        }
                    } else {
                        ASSERT_E(scu32(data.values.nums[idx - 2]) + scu32(data.values.nums[idx - 1]) + scu32(data.values.nums[idx]), (wordSize * wordSize * 3))
                        ASSERT_NE(_size, numChildren)
                        allocate(false, true, _size, meta, lowerChild, key, --idx);
                    }
                }
                    break;

            }
        }
        ASSERT_LE(data.values.nums[idx - 1], wordSize * wordSize)
        ASSERT_LE(data.values.nums[idx], wordSize * wordSize)
        ASSERT_PLE(idx + 1 < _size, data.values.nums[idx + 1], wordSize * wordSize)
        ASSERT_CODE_LOCALE(for(int i = 0; i < _size - (_size == numChildren); ++i) {
            ASSERT_R(data.values.nums[i], wordSize * wordSize / 2, wordSize * wordSize + 1)
            ASSERT_R(RCNODE_0(getChildrenPtr())[getIdx(meta, i)].template rank<true>(data.values.nums[i] - (data.values.nums[i] == wordSize * wordSize)), data.values.ones[i] - 1, data.values.ones[i] + 1, i)
        })
        denormalize(_size);
        writeMetaData<false>(meta);
        setSize(_size);
        if constexpr(tryAlloc)
            return out;
    }

    template<bool tryAlloc, int lowerLevel, bool isLowest>
    inline std::conditional_t<tryAlloc, bool, void> allocate(uint8_t *&lowerChild, uint_fast8_t &idx, uint_fast8_t &lowerIndex, const uint64_t upperFullNums, const uint64_t upperFullOnes, uint64_t &lowerFullNums, uint64_t &lowerFullOnes) {
        static_assert(lowerLevel != 0);
        ASSERT_E(rcuint(&RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(readMetaData<false>(), idx)]), rcuint(lowerChild))
        ASSERT_E(scu32(RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(readMetaData<false>(), idx)].size()), NODE_UN(lowerLevel)::numChildren)
        bool out = true;
        uint64_t meta = readMetaData<false>();
        uint_fast8_t _size = size();
        clearMetadata();
        uint16_t lastNums = upperFullNums - getSumExceptLast<true>();
        uint16_t lastOnes = upperFullOnes - getSumExceptLast<false>();
        normalize(_size);
        const auto realAllocate = [this, &lowerChild, &idx, &lowerIndex, &_size, &meta, &lowerFullNums, &lowerFullOnes, &lastNums, &lastOnes](bool lowest, bool highest) -> bool {
            if(_size == numChildren) {
                if(lowest)--idx;
                else if(highest)++idx;
                if constexpr(!tryAlloc) { ASSERT_UNREACHEABLE("cannot happen") }
                return false;
            }
            ASSERT_R(scu32(idx), 1, numChildren - 2)
            ASSERT_B(!lowest || !highest)
            NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
            NODE_UN(lowerLevel) *mChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx)];
            NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)];
            ASSERT_R(lChild->size(), NODE_UN(lowerLevel)::numChildren - 1, NODE_UN(lowerLevel)::numChildren + 1)
            ASSERT_PE(!lowest && !highest, mChild->size(), NODE_UN(lowerLevel)::numChildren)
            ASSERT_R(rChild->size(), NODE_UN(lowerLevel)::numChildren - 1, NODE_UN(lowerLevel)::numChildren + 1)
            lastNums = data.values.nums[numChildren - 2];
            lastOnes = data.values.ones[numChildren - 2];
            for(int i = _size - (_size == numChildren - 1); i > idx; --i) {
                data.values.nums[i] = data.values.nums[i - 1];
                data.values.ones[i] = data.values.ones[i - 1];
            }
            meta = pInsert(meta, idx);
            uint_fast8_t delta1 = (NODE_UN(lowerLevel)::numChildren / 4) + lowest, delta2 = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren, 4), delta3 = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren, 4) - lowest;
            NODE_UN(lowerLevel) *nChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx)];
            data.values.nums[idx] = 0, data.values.ones[idx] = 0;
            uint_fast8_t tIdx = idx - 1, tLIdx = lowerIndex;
            uint8_t *tLChild = lowerChild;
            nChild->template moveInit<uint16_t, isLowest>(lChild, delta1, lChild->size(), tLChild, tIdx, tLIdx, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
            mChild->template moveLeft<uint16_t, isLowest>(nChild, delta2 + delta3, delta1, mChild->size(), data.values.nums[idx], data.values.nums[idx + 1], data.values.ones[idx], data.values.ones[idx + 1]);
            if(idx != numChildren - 3) {
                rChild->template moveLeft<uint16_t, isLowest>(mChild, delta3, mChild->size(), rChild->size(), data.values.nums[idx + 1], data.values.nums[idx + 2], data.values.ones[idx + 1], data.values.ones[idx + 2]);
            } else {
                rChild->template moveLeft<uint16_t, isLowest>(mChild, delta3, mChild->size(), rChild->size(), data.values.nums[idx + 1], lastNums, data.values.ones[idx + 1], lastOnes);
            }
            if(lowest) {
                ASSERT_RI(tIdx, 0, 1)
                idx = tIdx;
                lowerIndex = tLIdx;
                lowerChild = tLChild;
            } else if(highest) {
                if(lowerIndex < delta3) {
                    ++idx;
                    lowerIndex += mChild->size() - (delta3);
                    lowerChild = rcbyte(&RCNODE_2(getChildrenPtr())[getIdx(meta, idx)]);
                } else {
                    lowerIndex -= delta3;
                    idx += 2;
                    ASSERT_E(rcuint(lowerChild), rcuint(&RCNODE_2(getChildrenPtr())[getIdx(meta, idx)]))
                }
            } else {
                if(lowerIndex < delta2 + delta3) {
                    lowerIndex += delta1;
                    lowerChild = rcbyte(&RCNODE_2(getChildrenPtr())[getIdx(meta, idx)]);
                } else {
                    lowerIndex -= (delta2 + delta3);
                    ++idx;
                    ASSERT_E(rcuint(lowerChild), rcuint(&RCNODE_2(getChildrenPtr())[getIdx(meta, idx)]))
                }
            }
            if(idx != NODE_UN(lowerLevel)::numChildren - 1) {
                lowerFullNums = data.values.nums[idx];
                lowerFullOnes = data.values.ones[idx];
            } else {
                lowerFullNums = lastNums;
                lowerFullOnes = lastOnes;
            }
            ++_size;
            return true;
        };
        //einfügen möglich
        if(RANGE(idx, 1, _size - 1)) {
            uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)].size(), rSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)].size();
            NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
            NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)];
            if(idx != numChildren - 2) {
                if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                    uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                    delta -= (lSize < delta);
                    RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint16_t, isLowest>(lChild, delta, lSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
                    if(lowerIndex < delta) {
                        lowerIndex += lSize;
                        --idx;
                        lowerChild = rcbyte(lChild);
                    } else {
                        lowerIndex -= delta;
                    }
                } else if(rSize < NODE_UN(lowerLevel)::numChildren - 1) {
                    const uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - rSize, 2);
                    RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint16_t, isLowest>(rChild, delta, numChildren, rSize, data.values.nums[idx], data.values.nums[idx + 1], data.values.ones[idx], data.values.ones[idx + 1]);
                    if(lowerIndex >= numChildren - delta) {
                        lowerIndex -= numChildren - delta;
                        ++idx;
                        lowerChild = rcbyte(rChild);
                    }
                } else {
                    out = realAllocate(false, false);
                }
                lowerFullNums = data.values.nums[idx];
                lowerFullOnes = data.values.ones[idx];
            } else {
                if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                    uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                    delta -= (lSize < delta);
                    RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint16_t, isLowest>(lChild, delta, lSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
                    if(lowerIndex < delta) {
                        lowerIndex += lSize;
                        --idx;
                        lowerChild = rcbyte(lChild);
                    } else {
                        lowerIndex -= delta;
                    }
                    lowerFullNums = data.values.nums[idx];
                    lowerFullOnes = data.values.ones[idx];
                } else if(rSize < NODE_UN(lowerLevel)::numChildren - 1) {
                    const uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - rSize, 2);
                    RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint16_t, isLowest>(rChild, delta, numChildren, rSize, data.values.nums[idx], lastNums, data.values.ones[idx], lastOnes);
                    if(lowerIndex >= numChildren - delta) {
                        lowerIndex -= numChildren - delta;
                        ++idx;
                        lowerChild = rcbyte(rChild);
                        lowerFullNums = lastNums;
                        lowerFullOnes = lastOnes;
                    } else {
                        lowerFullNums = data.values.nums[numChildren - 2];
                        lowerFullOnes = data.values.ones[numChildren - 2];
                    }
                } else {
                    if constexpr(tryAlloc) { out = false; } else { ASSERT_UNREACHEABLE("cant happen"); }
                }
            }
        } else {
            switch(idx) {
                case 0: {
                    if(_size != 2) {
                        NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)];
                        NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 2)];
                        uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)].size(), rSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 2)].size();
                        if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                            const uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                            RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint16_t, isLowest>(lChild, delta, numChildren, lSize, data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                            if(lowerIndex >= numChildren - delta) {
                                lowerIndex -= numChildren - delta;
                                idx = 1;
                                lowerChild = rcbyte(lChild);
                            }
                        } else if(rSize + lSize < (2 * NODE_UN(lowerLevel)::numChildren) - 1) {
                            uint_fast8_t delta1 = DIV_ROUNDUP((2 * NODE_UN(lowerLevel)::numChildren )- (lSize+rSize), 3),
                                    delta2 = ((2 * NODE_UN(lowerLevel)::numChildren) - (lSize + rSize)) / 3;
                            //delta2 += (rSize == (NODE_UN(lowerLevel)::numChildren - 1));
                            if(idx > NODE_UN(lowerLevel)::numChildren - delta2) {
                                swap(delta1, delta2);
                            }
                            lChild->template moveRight<uint16_t, isLowest>(rChild, delta1 + delta2, lSize, rSize, data.values.nums[1], data.values.nums[2], data.values.ones[1], data.values.ones[2]);
                            RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint16_t, isLowest>(lChild, delta1, NODE_UN(lowerLevel)::numChildren, lSize - (delta1 + delta2), data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                            if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta1) {
                                lowerIndex -= NODE_UN(lowerLevel)::numChildren - delta1;
                                idx = 1;
                                lowerChild = rcbyte(lChild);
                            }
                        } else {
                            ++idx;
                            out = realAllocate(true, false);
                        }
                    } else {
                        uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)].size();
                        if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                            NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)];
                            uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                            delta -= (idx >= NODE_UN(lowerLevel)::numChildren - delta);
                            RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint16_t, isLowest>(lChild, delta, NODE_UN(lowerLevel)::numChildren, lSize, data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                            if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta) {
                                lowerIndex -= NODE_UN(lowerLevel)::numChildren - delta;
                                idx = 1;
                                lowerChild = rcbyte(lChild);
                            }
                        } else {
                            NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)];
                            NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 2)];
                            uint_fast8_t delta1 = DIV_ROUNDUP((2 * NODE_UN(lowerLevel)::numChildren )- (lSize), 3),
                                    delta2 = ((2 * NODE_UN(lowerLevel)::numChildren) - (lSize)) / 3;
                            data.values.nums[2] = 0, data.values.ones[2] = 0;
                            uint8_t *throwaway;
                            uint_fast8_t throwAway2, throwAway3;
                            rChild->template moveInit<uint16_t, isLowest>(lChild, delta1 + delta2, lSize, throwaway, throwAway2, throwAway3, data.values.nums[1], data.values.nums[2], data.values.ones[1], data.values.ones[2]);
                            RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint16_t, isLowest>(lChild, delta1, numChildren, lSize - (delta1 + delta2), data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                            ++_size;
                            if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - (delta1)) {
                                lowerIndex -= NODE_UN(lowerLevel)::numChildren - (delta1);
                                idx = 1;
                                lowerChild = rcbyte(lChild);
                            } else {
                                idx = 0;
                            }
                        }
                    }
                    lowerFullNums = data.values.nums[idx];
                    lowerFullOnes = data.values.ones[idx];
                    break;
                }
                case 1: {
                    uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 0)].size();
                    if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                        NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 0)];
                        uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                        delta -= (lSize < delta);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint16_t, isLowest>(lChild, delta, lSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                        if(lowerIndex < delta) {
                            lowerIndex += lSize;
                            idx = 0;
                            lowerChild = rcbyte(lChild);
                        } else {
                            lowerIndex -= delta;
                        }
                    } else {
                        NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 2)];
                        uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren, 2);
                        delta -= (lSize < delta);
                        data.values.nums[2] = 0, data.values.ones[2] = 0;
                        rChild->template moveInit<uint16_t, isLowest>(RCNODE_UN(lowerLevel)(lowerChild), delta, NODE_UN(lowerLevel)::numChildren, lowerChild, idx, lowerIndex, data.values.nums[1], data.values.nums[2], data.values.ones[1], data.values.ones[2]);
                        ++_size;
                        if(lowerIndex >= numChildren - delta) {
                            ASSERT_E(idx, 2)
                            ASSERT_E(rcuint(lowerChild), rcuint(rChild))
                        }
                    }
                    lowerFullNums = data.values.nums[idx];
                    lowerFullOnes = data.values.ones[idx];
                    break;
                }
                case numChildren - 1: {
                    NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 3)];
                    NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 2)];
                    uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 3)].size(), rSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 2)].size();
                    if(rSize < NODE_UN(lowerLevel)::numChildren - 1) {
                        uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - rSize, 2);
                        delta -= (rSize < delta);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint16_t, isLowest>(rChild, delta, rSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[numChildren - 2], lastNums, data.values.ones[numChildren - 2], lastOnes);
                        if(lowerIndex < delta) {
                            lowerIndex += rSize;
                            idx = numChildren - 2;
                            lowerChild = rcbyte(rChild);
                            lowerFullNums = data.values.nums[numChildren - 2];
                            lowerFullOnes = data.values.ones[numChildren - 2];
                        } else {
                            lowerIndex -= delta;
                            lowerFullNums = lastNums;
                            lowerFullOnes = lastOnes;
                        }
                    } else if(lSize + rSize < (2 * NODE_UN(lowerLevel)::numChildren) - 1) {
                        uint_fast8_t delta1 = ((2 * NODE_UN(lowerLevel)::numChildren) - (lSize + rSize)) / 3, delta2 = DIV_ROUNDUP((2 * NODE_UN(lowerLevel)::numChildren )- (lSize+rSize), 3);
                        if(idx < delta1 - 1) {
                            swap(delta1, delta2);
                        }
                        rChild->template moveLeft<uint16_t, isLowest>(lChild, delta1 + delta2, lSize, rSize, data.values.nums[numChildren - 3], data.values.nums[numChildren - 2], data.values.ones[numChildren - 3], data.values.ones[numChildren - 2]);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint16_t, isLowest>(rChild, delta2, rSize - (delta1 + delta2), numChildren, data.values.nums[numChildren - 2], lastNums, data.values.ones[numChildren - 2], lastOnes);
                        if(lowerIndex < delta2) {
                            lowerIndex += rSize - (delta1 + delta2);
                            --idx;
                            lowerChild = rcbyte(rChild);
                            lowerFullNums = data.values.nums[numChildren - 2];
                            lowerFullOnes = data.values.ones[numChildren - 2];
                        } else {
                            lowerIndex -= delta2;
                            lowerFullNums = lastNums;
                            lowerFullOnes = lastOnes;
                        }
                    } else {
                        out = false;
                    }
                    break;
                }
                default: {
                    NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 2)];
                    NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
                    uint_fast8_t lSize = lChild->size(), rSize = rChild->size();
                    if(rSize < NODE_UN(lowerLevel)::numChildren - 1) {
                        uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - rSize, 2);
                        delta -= (lSize < delta);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint16_t, isLowest>(rChild, delta, rSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
                        if(lowerIndex < delta) {
                            lowerIndex += rSize;
                            --idx;
                            lowerChild = rcbyte(rChild);
                        } else {
                            lowerIndex -= delta;
                        }
                    } else if(lSize + rSize < (2 * NODE_UN(lowerLevel)::numChildren) - 1) {
                        uint_fast8_t delta1 = ((2 * NODE_UN(lowerLevel)::numChildren) - (lSize + rSize)) / 3, delta2 = DIV_ROUNDUP((2 * NODE_UN(lowerLevel)::numChildren )- (lSize+rSize), 3);
                        if(idx < delta1 - 1) {
                            swap(delta1, delta2);
                        }
                        rChild->template moveLeft<uint16_t, isLowest>(lChild, delta1 + delta2, lSize, rSize, data.values.nums[idx - 2], data.values.nums[idx - 1], data.values.ones[idx - 2], data.values.ones[idx - 1]);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint16_t, isLowest>(rChild, delta2, rSize - (delta1 + delta2), numChildren, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
                        if(lowerIndex < delta2) {
                            lowerIndex += rSize - (delta1 + delta2);
                            --idx;
                            lowerChild = rcbyte(rChild);
                        } else {
                            lowerIndex -= delta2;
                        }
                    } else {
                        --idx;
                        out = realAllocate(false, true);
                    }
                    lowerFullNums = data.values.nums[idx];
                    lowerFullOnes = data.values.ones[idx];
                    break;
                }
            }
        }
        denormalize(_size);
        writeMetaData<false>(meta);
        setSize(_size);
        if constexpr(tryAlloc) return out;
    }


    inline bool lowestDeallocate(bool isOne, uint64_t fullNums, uint64_t curNums, uint64_t &key, uint_fast8_t &idx) {
        uint_fast8_t _size = size();
        uint64_t meta = readMetaData<false>();
        clearMetadata();
        uint16_t lastNums = fullNums - getSumExceptLast<true>();
        normalize(_size);
        bool out = false;
        const auto merge = [this, &idx, &_size, &meta, &out](uint16_t delta) {
            ASSERT_RE(idx, 0, numChildren - 1)
            ASSERT_E(data.values.nums[idx] + delta, wordSize * wordSize)
            ASSERT_PE(idx != numChildren - 2, data.values.nums[idx + 1] - delta, 0)
            NODE0 *lChild = &RCNODE_0(getChildrenPtr())[getIdx(meta, idx)];
            NODE0 *rChild = &RCNODE_0(getChildrenPtr())[getIdx(meta, idx + 1)];
            uint16_t ones = rChild->template rank<true>(delta);
            rChild->moveLeft(lChild, data.values.nums[idx], delta);
            data.values.nums[idx] = wordSize * wordSize;
            data.values.ones[idx] += ones;
            for(int i = idx + 1; i < _size; ++i) {
                data.values.nums[idx] = data.values.nums[idx + 1];
                data.values.ones[idx] = data.values.ones[idx + 1];
            }
            meta = pRemove(meta, idx);
            --_size;
            data.values.nums[_size] = 0xffff;
            data.values.ones[_size] = 0xffff;
            out = true;
        };
        RCNODE_0(getChildrenPtr())[getIdx(meta, idx)].remove(key);
        if(idx != numChildren - 1) {
            --data.values.nums[idx];
            data.values.ones[idx] -= isOne;
        }
        switch(idx) {
            case 0:
                if(curNums + data.values.nums[1] <= wordSize * wordSize) {
                    merge(data.values.nums[1]);
                }
                break;
            case numChildren - 1:
                if(curNums + data.values.nums[numChildren - 2] <= wordSize * wordSize) {
                    merge(curNums);
                }
                break;
            case numChildren - 2:
                if(lastNums < data.values.nums[numChildren - 3]) {
                    if(curNums + lastNums <= wordSize * wordSize) {
                        merge(lastNums);
                    }
                } else {
                    if(curNums + data.values.nums[numChildren - 3] <= wordSize * wordSize) {
                        idx = numChildren - 3;
                        merge(curNums);
                    }
                }
                break;
            default:
                if(idx == _size - 1) {
                    if(curNums + data.values.nums[idx - 1] <= wordSize * wordSize) {
                        --idx;
                        merge(idx);
                    }
                } else {
                    if(data.values.nums[idx + 1] < data.values.nums[idx - 1]) {
                        if(curNums + data.values.nums[idx + 1] <= wordSize * wordSize) {
                            merge(idx);
                        }
                    } else {
                        if(curNums + data.values.nums[numChildren - 3] <= wordSize * wordSize) {
                            --idx;
                            merge(idx);
                        }
                    }
                }
                break;
        }
        ASSERT_NE(idx != numChildren - 1, data.values.nums[idx], 0)
        clearMetadata();
        denormalize(_size);
        writeMetaData<false>(meta);
        setSize(_size);
        return out;
    }

    template<bool isLowest, int lowerLevel>
    inline bool deallocate(bool isOne, uint64_t fullNums, uint64_t fullOnes, uint64_t curNums, uint64_t curOnes, uint_fast8_t &idx) {
        static_assert(lowerLevel == 2);
        ASSERT_E(RCNODE_UN(lowerLevel)(getChildrenPtr())[idx].size(), NODE_UN(lowerLevel)::numChildren)
        uint_fast8_t _size = size();
        bool out = false;
        uint64_t meta = readMetaData<false>();
        clearMetadata();
        uint16_t lastNums = fullNums - getSumExceptLast<true>();
        uint16_t lastOnes = fullOnes - getSumExceptLast<false>();
        normalize(_size);
        const auto merge = [this, &idx, &_size, &meta, &out](NODE_UN(lowerLevel) *lNode, NODE_UN(lowerLevel) *rNode, uint_fast8_t lSize, uint_fast8_t rSize, uint16_t &lNums, uint16_t &lOnes, uint16_t &rNums, uint16_t &rOnes) {
            ASSERT_NE(lSize + rSize, NODE_UN(lowerLevel)::numChildren)
            ASSERT_NE(rcuint(&lNode), rcuint(&rNode))
            rNode->template moveLeft<uint16_t, isLowest>(lNode, rSize, lSize, rSize, lNums, lOnes, rNums, rOnes);
            for(int i = idx + 1; i < _size; ++i) {
                data.values.nums[i] = data.values.nums[i + 1];
                data.values.ones[i] = data.values.ones[i + 1];
            }
            --_size;
            data.values.nums[_size] = 0xffff;
            data.values.ones[_size] = 0xffff;
            meta = pRemove(meta, idx);
            out = true;
        };
        if(idx != numChildren - 1) {
            --data.values.nums[idx];
            data.values.ones[idx] -= isOne;
        }
        switch(idx) {
            case 0: {
                NODE_UN(lowerLevel) *lNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 0)];
                NODE_UN(lowerLevel) *rNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)];
                const uint_fast8_t lSize = lNode->size(), rSize = rNode->size();
                if(lSize + rSize <= NODE_UN(lowerLevel)::numChildren) {
                    merge(lNode, rNode, lSize, rSize, data.values.nums[0], data.values.ones[0], data.values.nums[1], data.values.ones[1]);
                }
                break;
            }
            case numChildren - 1: {
                NODE_UN(lowerLevel) *lNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 2)];
                NODE_UN(lowerLevel) *rNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 1)];
                const uint_fast8_t lSize = lNode->size(), rSize = rNode->size();
                if(lSize + rSize <= NODE_UN(lowerLevel)::numChildren) {
                    merge(lNode, rNode, lSize, rSize, data.values.nums[numChildren - 2], data.values.ones[numChildren - 2], lastNums, lastOnes);
                    --idx;
                }
                break;
            }
            case numChildren - 2: {
                NODE_UN(lowerLevel) *lNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
                NODE_UN(lowerLevel) *mNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx)];
                NODE_UN(lowerLevel) *rNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)];
                const uint_fast8_t lSize = lNode->size(), mSize = mNode->size(), rSize = rNode->size();
                if(lSize < rSize) {
                    if(lSize + mSize <= NODE_UN(lowerLevel)::numChildren) {
                        merge(lNode, mNode, lSize, mSize, data.values.nums[idx - 1], data.values.ones[idx - 1], data.values.nums[idx], data.values.ones[idx]);
                        --idx;
                    }
                } else {
                    if(mSize + rSize <= NODE_UN(lowerLevel)::numChildren) {
                        merge(mNode, rNode, mSize, rSize, data.values.nums[idx], data.values.ones[idx], lastNums, lastOnes);
                    }
                }
                break;
            }
            default: {
                NODE_UN(lowerLevel) *lNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
                NODE_UN(lowerLevel) *mNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx)];
                NODE_UN(lowerLevel) *rNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)];
                const uint_fast8_t lSize = lNode->size(), mSize = mNode->size(), rSize = rNode->size();
                if(idx != _size - 1) {
                    if(lSize < rSize) {
                        if(lSize + mSize <= NODE_UN(lowerLevel)::numChildren) {
                            merge(lNode, mNode, lSize, mSize, data.values.nums[idx - 1], data.values.ones[idx - 1], data.values.nums[idx], data.values.ones[idx]);
                            --idx;
                        }
                    } else {
                        if(mSize + rSize <= NODE_UN(lowerLevel)::numChildren) {
                            merge(mNode, rNode, mSize, rSize, data.values.nums[idx], data.values.ones[idx], data.values.nums[idx + 1], data.values.ones[idx + 1]);
                        }
                    }
                } else {
                    if(lSize + mSize <= NODE_UN(lowerLevel)::numChildren) {
                        merge(lNode, mNode, lSize, mSize, data.values.nums[idx - 1], data.values.ones[idx - 1], data.values.nums[idx], data.values.ones[idx]);
                        --idx;
                    }
                }
                break;
            }
        }
        clearMetadata();
        denormalize(_size);
        writeMetaData<false>(meta);
        setSize(_size);
        return out;
    }

    inline constexpr void setChildrenPtr(uint8_t *ptr) {
        ASSERT_E(rc<uintptr_t>(ptr) & bitMasks[0], 0)
        data.values.childrenPtr = rcbyte(rcuint(data.values.childrenPtr) & bitMasks[0]);
        data.values.childrenPtr = rcbyte(rcuint(data.values.childrenPtr) | rcuint(ptr));
        ASSERT_E(getChildrenPtr(), ptr)
    }


    inline constexpr uint8_t *getChildrenPtr() const {
        return rcbyte(rc<uintptr_t>(data.values.childrenPtr) & ~bitMasks[0]);
    }

    [[nodiscard]]  inline constexpr uint_fast8_t size() const {
        if constexpr(useBinarySearch) {
            ASSERT_RI(readMetaData<true>() >> 60, 0, numChildren)
            return (readMetaData<true>() >> 60);
        } else {
            ASSERT_R(readMetaData<true>() >> 62, 0, 4)
            uint_fast8_t out = ((data.values.nums[7] & 0x3fff) != 0x3fff) << 3;
            out += ((data.values.nums[out + 3] & 0x3fff) != 0x3fff) << 2;
            out += ((readMetaData<true>() >> 62));
            // uint_fast8_t out = ((readMetaData<true>() >> 60));
            // out += ((data.values.nums[out + 2] & 0x3fff) != 0x3fff) << 1;
            // out += ((data.values.nums[out + 1] & 0x3fff) != 0x3fff);
            return out;
        }
    }

    inline constexpr void setSize(uint_fast8_t newSize) {
        // ASSERT_R(scu32(newSize), 2, numChildren + 1)
        ASSERT_CODE_LOCALE({
                               for(int i = newSize; i < numChildren - 1; ++i) {
                                   ASSERT_E((data.values.nums[i] & 0x1fff), 0x1fff, i)
                               }
                           })
        ASSERT_PE(useBinarySearch, rcuint(getChildrenPtr()) & bitMasks[0], 0)
        if constexpr(useBinarySearch) {
            writeMetaData<true>(scu64(newSize & 0b1111) << 60);
            ASSERT_E(readMetaData<true>() >> 60, scu64(newSize & 0b1111))
            ASSERT_E(scu32(size()), scu32(newSize))
        } else {
            writeMetaData<true>(scu64(newSize & 0b11) << 62);
            if(newSize < 7) {
                data.values.nums[7] |= 0x3fff;
                if(newSize < 3) data.values.nums[3] |= 0x3fff;
            } else if(newSize < 11)
                data.values.nums[11] |= 0x3fff;
            //  writeMetaData<true>(scu64(newSize & 0b1100) << 60);
            //  ASSERT_E(readMetaData<true>() >> 60, scu64(newSize & 0b1100))
            //  ASSERT_E(scu32(size()), scu32(newSize))

        }
    }

    inline uint_fast8_t getIdx(uint64_t &key) const {
        if constexpr(useBinarySearch) {
            //{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14}
            //{0,1,0,2,0,1,0,3,0,1,0 ,2 ,0 ,1 }
            if(key < (data.values.nums[7])) {
                //{0,1,2,3,4,5,6,7}
                if(key < (data.values.nums[3] & 0x7fff)) {
                    //{0,1,2,3}
                    if(key < (data.values.nums[1] & 0x3fff)) {
                        //{0,1}
                        if(key < (data.values.nums[0] & 0x1fff)) {
                            return 0;
                        } else {
                            key -= (data.values.nums[0] & 0x1fff);
                            return 1;
                        }
                    } else {
                        //{2,3}
                        if((key -= (data.values.nums[1] & 0x3fff)) < (data.values.nums[2] & 0x1fff)) {
                            return 2;
                        } else {
                            key -= data.values.nums[2] & 0x1fff;
                            return 3;
                        }
                    }
                } else {
                    //{4,5,6,7}
                    if((key -= (data.values.nums[3] & 0x7fff)) < (data.values.nums[5] & 0x3fff)) {
                        //{4,5}
                        if(key < (data.values.nums[4] & 0x1fff)) {
                            return 4;
                        } else {
                            key -= (data.values.nums[4] & 0x1fff);
                            return 5;
                        }
                    } else {
                        //{6,7}
                        if((key -= (data.values.nums[5] & 0x3fff)) < (data.values.nums[6] & 0x1fff)) {
                            return 6;
                        } else {
                            key -= (data.values.nums[6] & 0x1fff);
                            return 7;
                        }
                    }
                }
            } else {
                //{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14}
                //{0,1,0,2,0,1,0,3,0,1,0 ,2 ,0 ,1 }
                //{8,9,10,11,12,13,14}
                if((key -= data.values.nums[7]) < (data.values.nums[11] & 0x7fff)) {
                    //{8,9,10,11}
                    if(key < (data.values.nums[9] & 0x3fff)) {
                        //{8,9}
                        if(key < (data.values.nums[8] & 0x1fff)) {
                            return 8;
                        } else {
                            key -= (data.values.nums[8] & 0x1fff);
                            return 9;
                        }
                    } else {
                        //{10,11}
                        if((key -= (data.values.nums[9] & 0x3fff)) < (data.values.nums[10] & 0x1fff)) {
                            return 10;
                        } else {
                            key -= (data.values.nums[10] & 0x1fff);
                            return 11;
                        }
                    }
                } else {
                    //{12,13,14}
                    if((key -= (data.values.nums[11] & 0x7fff)) < (data.values.nums[13] & 0x3fff)) {
                        //{12,13}
                        if(key < (data.values.nums[12] & 0x1fff)) {
                            return 12;
                        } else {
                            key -= (data.values.nums[12] & 0x1fff);
                            return 13;
                        }
                    } else {
                        //{14}
                        key -= (data.values.nums[13] & 0x3fff);
                        return 14;
                    }
                }
            }
        } else {
            uint_fast8_t i = 0;
            uint32_t prevkey = key;
            for(; (key -= (data.values.nums[i] & 0x3fff)) < prevkey; ++i, prevkey = key);
            key = prevkey;
            return i;
        }
    }


    inline uint_fast8_t getIdx(uint64_t &key, uint64_t &curNums, uint64_t &curOnes) const {
        //TODO: curNums and curOnes need other update. result is false calc.
        if constexpr(useBinarySearch) {
            //{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14}
            //{0,1,0,2,0,1,0,3,0,1,0 ,2 ,0 ,1 }
            if(key <= (data.values.nums[7])) {
                //{0,1,2,3,4,5,6,7}
                if(key <= (data.values.nums[3] & 0x7fff)) {
                    //{0,1,2,3}
                    if(key <= (data.values.nums[1] & 0x3fff)) {
                        //{0,1}
                        if(key <= (data.values.nums[0] & 0x1fff)) {
                            curNums = data.values.nums[0] & 0x1fff;
                            curOnes = data.values.ones[0] & 0x1fff;
                            return 0;
                        } else {
                            curNums = (data.values.nums[1] & 0x3fff) - (data.values.nums[0] & 0x1fff);
                            curOnes = (data.values.ones[1] & 0x3fff) - (data.values.ones[0] & 0x1fff);
                            key -= (data.values.nums[0] & 0x1fff);
                            return 1;
                        }
                    } else {
                        //{2,3}
                        if((key -= (data.values.nums[1] & 0x3fff)) <= (data.values.nums[2] & 0x1fff)) {
                            curNums = data.values.nums[2] & 0x1fff;
                            curOnes = data.values.ones[2] & 0x1fff;
                            return 2;
                        } else {
                            curNums = (data.values.nums[3] & 0x7fff) - (data.values.nums[1] & 0x3fff) - (data.values.nums[2] & 0x1fff);
                            curOnes = (data.values.ones[3] & 0x7fff) - (data.values.ones[1] & 0x3fff) - (data.values.ones[2] & 0x1fff);
                            key -= data.values.nums[2] & 0x1fff;
                            return 3;
                        }
                    }
                } else {
                    //{4,5,6,7}
                    if((key -= (data.values.nums[3] & 0x7fff)) <= (data.values.nums[5] & 0x3fff)) {
                        //{4,5}
                        if(key <= (data.values.nums[4] & 0x1fff)) {
                            curNums = data.values.nums[4] & 0x1fff;
                            curOnes = data.values.ones[4] & 0x1fff;
                            return 4;
                        } else {
                            curNums = (data.values.nums[5] & 0x3fff) - (data.values.nums[4] & 0x1fff);
                            curOnes = (data.values.ones[5] & 0x3fff) - (data.values.ones[4] & 0x1fff);
                            key -= (data.values.nums[4] & 0x1fff);
                            return 5;
                        }
                    } else {
                        //{6,7}
                        if((key -= (data.values.nums[5] & 0x3fff)) <= (data.values.nums[6] & 0x1fff)) {
                            curNums = (data.values.nums[6] & 0x1fff);
                            curOnes = (data.values.ones[6] & 0x1fff);
                            return 6;
                        } else {
                            curNums = (curNums = data.values.nums[7]) - (data.values.nums[3] & 0x7fff) - (data.values.nums[5] & 0x3fff) - (data.values.nums[6] & 0x1fff);
                            curOnes = (curOnes = data.values.ones[7]) - (data.values.ones[3] & 0x7fff) - (data.values.ones[5] & 0x3fff) - (data.values.ones[6] & 0x1fff);
                            key -= (data.values.nums[6] & 0x1fff);
                            return 7;
                        }
                    }
                }
            } else {
                //{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14}
                //{0,1,0,2,0,1,0,3,0,1,0 ,2 ,0 ,1 }
                //{8,9,10,11,12,13,14}
                if((key -= data.values.nums[7]) <= (data.values.nums[11] & 0x7fff)) {
                    //{8,9,10,11}
                    if(key <= (data.values.nums[9] & 0x3fff)) {
                        //{8,9}
                        if(key <= (data.values.nums[8] & 0x1fff)) {
                            curNums = data.values.nums[8] & 0x1fff;
                            curOnes = data.values.ones[8] & 0x1fff;
                            return 8;
                        } else {
                            curNums = (data.values.nums[9] & 0x3fff) - (data.values.nums[8] & 0x1fff);
                            curOnes = (data.values.ones[9] & 0x3fff) - (data.values.ones[8] & 0x1fff);
                            key -= (data.values.nums[8] & 0x1fff);
                            return 9;
                        }
                    } else {
                        //{10,11}
                        if((key -= (data.values.nums[9] & 0x3fff)) <= (data.values.nums[10] & 0x1fff)) {
                            curNums = data.values.nums[10] & 0x1fff;
                            curOnes = data.values.ones[10] & 0x1fff;
                            return 10;
                        } else {
                            curNums = (data.values.nums[11] & 0x7fff) - (data.values.nums[9] & 0x3fff) - (data.values.nums[10] & 0x1fff);
                            curOnes = (data.values.ones[11] & 0x7fff) - (data.values.ones[9] & 0x3fff) - (data.values.ones[10] & 0x1fff);
                            key -= (data.values.nums[10] & 0x1fff);
                            return 11;
                        }
                    }
                } else {
                    //{12,13,14}
                    if((key -= (data.values.nums[11] & 0x7fff)) <= (data.values.nums[13] & 0x3fff)) {
                        //{12,13}
                        if(key <= (data.values.nums[12] & 0x1fff)) {
                            curNums = data.values.nums[12] & 0x1fff;
                            curOnes = data.values.ones[12] & 0x1fff;
                            return 12;
                        } else {
                            curNums = (data.values.nums[13] & 0x3fff) - (data.values.nums[12] & 0x1fff);
                            curOnes = (data.values.ones[13] & 0x3fff) - (data.values.ones[12] & 0x1fff);
                            key -= (data.values.nums[12] & 0x1fff);
                            return 13;
                        }
                    } else {
                        //{14}
                        curNums -= (data.values.nums[7]) + (data.values.nums[11] & 0x7fff) + (data.values.nums[13] & 0x3fff);
                        curOnes -= (data.values.ones[7]) + (data.values.ones[11] & 0x7fff) + (data.values.ones[13] & 0x3fff);
                        key -= (data.values.nums[13] & 0x3fff);
                        return 14;
                    }
                }
            }
        } else {
            uint_fast8_t i = 0;
            uint32_t prevkey = key;
            for(; (key -= (data.values.nums[i] & 0x3fff)) < prevkey; ++i, prevkey = key) curNums -= data.values.nums[i] & 0x3fff;
            key = prevkey;
            return i;
        }
    }

    template<bool rank, bool one>
    inline pair<uint_fast8_t, uint64_t> select_rankIdx(uint64_t &key) const {
        //uint64_t meta = readMetaData<false>(),
        uint64_t cnums = 0;
        auto arrayFunction = [this](uint_fast8_t idx) constexpr {
            if constexpr(rank) {
                return data.values.nums[idx];
            } else {
                if constexpr(one) {
                    return data.values.ones[idx];
                } else {
                    return data.values.nums[idx] - data.values.ones[idx];
                }
            }
        };
        auto alterFunction = [this](uint_fast8_t idx) constexpr {
            if constexpr(!rank) {
                return data.values.nums[idx];
            } else {
                if constexpr(one) {
                    return data.values.ones[idx];
                } else {
                    return data.values.nums[idx] - data.values.ones[idx];
                }
            }
        };
        uint_fast8_t _size = size();
        if constexpr(useBinarySearch) {
            //{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14}
            //{0,1,0,2,0,1,0,3,0,1,0 ,2 ,0 ,1 }
            if((_size <= 8) | (key < (arrayFunction(7)))) {
                //{0,1,2,3,4,5,6,7}
                if((_size <= 4) | (key < (arrayFunction(3) & 0x7fff))) {
                    //{0,1,2,3}
                    if((_size <= 2) | (key < (arrayFunction(1) & 0x3fff))) {
                        //{0,1}
                        if((_size <= 1) | (key < (arrayFunction(0) & 0x1fff))) {
                            return {0, cnums};
                        } else {
                            cnums += alterFunction(0) & 0x1fff;
                            key -= (arrayFunction(0) & 0x1fff);
                            return {1, cnums};
                        }
                    } else {
                        //{2,3}
                        cnums += alterFunction(1) & 0x3fff;
                        ASSERT_GE(_size, 3)
                        if((key -= (arrayFunction(1) & 0x3fff)) < (arrayFunction(2) & 0x1fff)) {
                            return {2, cnums};
                        } else {
                            cnums += alterFunction(2) & 0x1fff;
                            key -= arrayFunction(2) & 0x1fff;
                            return {3, cnums};
                        }
                    }
                } else {
                    //{4,5,6,7}
                    cnums += alterFunction(3) & 0x7fff;
                    if((_size <= 6) | ((key -= (arrayFunction(3) & 0x7fff)) < (arrayFunction(5) & 0x3fff))) {
                        //{4,5}
                        if((_size <= 5) | (key < (arrayFunction(4) & 0x1fff))) {
                            return {4, cnums};
                        } else {
                            cnums += alterFunction(4) & 0x1fff;
                            key -= (arrayFunction(4) & 0x1fff);
                            return {5, cnums};
                        }
                    } else {
                        //{6,7}
                        cnums += alterFunction(5) & 0x3fff;
                        ASSERT_GE(_size, 7)
                        if((key -= (arrayFunction(5) & 0x3fff)) < (arrayFunction(6) & 0x1fff)) {
                            return {6, cnums};
                        } else {
                            cnums += alterFunction(6) & 0x1fff;
                            key -= (arrayFunction(6) & 0x1fff);
                            return {7, cnums};
                        }
                    }
                }
            } else {
                //{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14}
                //{0,1,0,2,0,1,0,3,0,1,0 ,2 ,0 ,1 }
                //{8,9,10,11,12,13,14}
                cnums += alterFunction(7);
                if((_size <= 12) | ((key -= arrayFunction(7)) < (arrayFunction(11) & 0x7fff))) {
                    //{8,9,10,11}
                    if((_size <= 10) | (key < (arrayFunction(9) & 0x3fff))) {
                        //{8,9}
                        if((_size <= 9) | (key < (arrayFunction(8) & 0x1fff))) {
                            return {8, cnums};
                        } else {
                            cnums += alterFunction(8) & 0x1fff;
                            key -= (arrayFunction(8) & 0x1fff);
                            return {9, cnums};
                        }
                    } else {
                        //{10,11}
                        ASSERT_GE(_size, 11)
                        cnums += alterFunction(9) & 0x3fff;
                        if((key -= (arrayFunction(9) & 0x3fff)) < (arrayFunction(10) & 0x1fff)) {
                            return {10, cnums};
                        } else {
                            cnums += alterFunction(10) & 0x1fff;
                            key -= (arrayFunction(10) & 0x1fff);
                            return {11, cnums};
                        }
                    }
                } else {
                    //{12,13,14}
                    cnums += alterFunction(11) & 0x7fff;
                    if((_size <= 14) | ((key -= (arrayFunction(11) & 0x7fff)) < (arrayFunction(13) & 0x3fff))) {
                        //{12,13}
                        if((_size <= 13) | (key < (arrayFunction(12) & 0x1fff))) {
                            return {12, cnums};
                        } else {
                            cnums += alterFunction(12) & 0x1fff;
                            key -= (arrayFunction(12) & 0x1fff);
                            return {13, cnums};
                        }
                    } else {
                        //{14}
                        cnums += alterFunction(13) & 0x3fff;
                        key -= (arrayFunction(13) & 0x3fff);
                        return {14, cnums};
                    }
                }
            }
        } else {
            uint_fast8_t i = 0;
            uint32_t prevkey = key;
            for(; (key -= (arrayFunction(i) & 0x3fff)) < prevkey; ++i, prevkey = key) cnums += alterFunction(i) & 0x3fff;
            key = prevkey;
            return {i, cnums};
        }
    }

    template<typename intType, bool isLowest>
    inline void moveInit(NODE2 *lnode, uint_fast8_t delta, uint_fast8_t lSize, uint8_t *&curChild, uint_fast8_t &upperindex, uint_fast8_t &index, intType &lNextNums, intType &rNextNums, intType &lNextOnes, intType &rNextOnes) {
        static_assert(is_integral_v<intType>);
        static_assert(!is_signed_v<intType>);
        ASSERT_R(scu32(lnode->size()), numChildren - 1, numChildren + 1)
        ASSERT_E(rNextNums, 0)
        ASSERT_E(rNextOnes, 0)
        using T = std::conditional_t<isLowest, NODE0, NODE2 >;
        T *leaves;
        if constexpr(isLowest)leaves = DYNAMIC_BV::allocateLeaves(); else leaves = DYNAMIC_BV::template allocateNode<2, 2>();
        const uint64_t thisMeta = 0xedc'ba98'7654'3210, lMeta = lnode->readMetaData<false>();
        lnode->clearMetadata();
        lnode->normalize(lSize);
        for(int i = delta; i < numChildren - 1; ++i) {
            data.values.nums[i] = 0xffff;
            data.values.ones[i] = 0xffff;
        }
        for(int i = 0; i < delta - 1; ++i) {
            const uint16_t curNums = lnode->data.values.nums[i + lSize - delta], curOnes = lnode->data.values.ones[i + lSize - delta];
            lnode->data.values.nums[i + lSize - delta] = 0xffff;
            lnode->data.values.ones[i + lSize - delta] = 0xffff;
            data.values.nums[i] = curNums;
            data.values.ones[i] = curOnes;
            lNextNums -= curNums;
            lNextOnes -= curOnes;
            rNextNums += curNums;
            rNextOnes += curOnes;
            std::memcpy(leaves + (getIdx(thisMeta, i)), lnode->getChildrenPtr() + (lnode->getIdx(lMeta, i + lSize - delta) * sizeof(T)), sizeof(T));
        }
        intType copyFullNums = lNextNums, copyFullOnes = lNextOnes;
        for(int i = 0; i < lSize - delta; ++i) {
            copyFullNums -= lnode->data.values.nums[i];
            copyFullOnes -= lnode->data.values.ones[i];
        }
        for(int i = lSize - delta; i < numChildren - 1; ++i) {
            lnode->data.values.nums[i] = 0xffff;
            lnode->data.values.ones[i] = 0xffff;
        }
        data.values.nums[delta - 1] = copyFullNums;
        data.values.ones[delta - 1] = copyFullOnes;
        lNextNums -= copyFullNums;
        lNextOnes -= copyFullOnes;
        rNextNums += copyFullNums;
        rNextOnes += copyFullOnes;
        if(index >= lSize - delta) {
            ++upperindex;
            index -= lSize - delta;
            curChild = rcbyte(this);
            //lowerChild = rcbyte(leaves + getIdx(thisMeta, index));
        }
        std::memcpy(leaves + (getIdx(thisMeta, delta - 1)), lnode->getChildrenPtr() + (lnode->getIdx(lMeta, lSize - 1) * sizeof(T)), sizeof(T));
        ASSERT_CODE_LOCALE(if constexpr(isLowest) {
            for(int i = 0; i < delta; ++i) {
                // printAll(RCNODE_0(leaves)[getIdx(thisMeta, i)].template rank<true>(data.values.nums[i] - (data.values.nums[i] == wordSize * wordSize)), data.values.ones[i] - 1, data.values.ones[i] + 1, i);
            }
            for(int i = 0; i < lSize - 1; ++i) {
                ASSERT_PR(data.values.nums[i] != 0xffff, data.values.nums[i], wordSize * wordSize / 2, wordSize * wordSize + 1)
            }
            for(int i = 0; i < delta; ++i) {
                ASSERT_R(RCNODE_0(leaves)[getIdx(thisMeta, i)].template rank<true>(data.values.nums[i] - (data.values.nums[i] == wordSize * wordSize)), sc64(data.values.ones[i]) - 1, data.values.ones[i] + 1, i)
            }
        } else {
            for(int i = 0; i < delta; ++i) {
                uint64_t *rval = rc<uint64_t *>(leaves) + (8 * getIdx(thisMeta, i));
                uint64_t *lval = rc<uint64_t *>(lnode->getChildrenPtr()) + (8 * lnode->getIdx(lMeta, i + lSize - delta));
                for(int j = 0; j < 8; ++j) {
                    ASSERT_E(rval[j], lval[j], i, j)
                }
            }
        })
        lnode->clearMetadata();
        lnode->denormalize(lSize - delta);
        lnode->setSize(lSize - delta);
        lnode->template writeMetaData<false>(lMeta);
        clearMetadata();
        denormalize(delta);
        setSize(delta);
        writeMetaData<false>(thisMeta);
        setChildrenPtr(rcbyte(leaves));
    }

    template<uint8_t length>
    inline void init(const uint16_t (&nums)[length], const uint16_t (&ones)[length]) {
        static_assert(length <= numChildren);
        int i = 0;
        for(; i < length; ++i) {
            ASSERT_GE(__builtin_clz(nums[i]), 16 + 3)
            ASSERT_GE(__builtin_clz(ones[i]), 16 + 3)
            data.values.nums[i] = nums[i];
            data.values.ones[i] = ones[i];
            ASSERT_PLE(useBinarySearch, data.values.nums[i], 0x1fff)
            ASSERT_PL(!useBinarySearch, data.values.nums[i], 0x3fff)
            ASSERT_PLE(useBinarySearch, data.values.ones[i], 0x1fff)
            ASSERT_PL(!useBinarySearch, data.values.ones[i], 0x3fff)
        }

        for(; i < NODE2::numChildren - 1; ++i) {
            data.values.nums[i] = 0xffff;
            data.values.ones[i] = 0xffff;
        }
        clearMetadata();
        denormalize(length);
        setSize(length);
        writeMetaData<false>(0xedc'ba98'7654'3210);
    }

    template<typename ArrType, typename SizeType>
    inline uint32_t init(NO_ASSERT_CODE(const) SameValueArray<ArrType, SizeType> &numsVec, const uint64_t *onesVec, uint64_t begin, uint16_t length) {
        ASSERT_B(isMetaCleared<true>() && isMetaCleared<false>())
        ASSERT_LE(length, numChildren)
        // ASSERT_CODE(uint32_t numCount = 0, oneCount = 0;)
        int i = 0;
        uint32_t out = 0;
        for(; i < length; ++i, ++begin) {
            ASSERT_GE(__builtin_clz(numsVec[begin]) - 16, 3)
            // ASSERT_GE(__builtin_clz(onesVec[begin]) - 16, 3)
            out += onesVec[begin];
            data.values.nums[i] = numsVec[begin];
            data.values.ones[i] = onesVec[begin];
            ASSERT_E(data.values.nums[i], numsVec[begin])
            ASSERT_E(data.values.ones[i], onesVec[begin] & 0xffff)
            ASSERT_PLE(useBinarySearch, data.values.nums[i], 0x1fff)
            ASSERT_PL(!useBinarySearch, data.values.nums[i], 0x3fff)
            ASSERT_PLE(useBinarySearch, data.values.ones[i], 0x1fff)
            ASSERT_PL(!useBinarySearch, data.values.ones[i], 0x3fff)
        }
        for(; i < NODE2::numChildren - 1; ++i) {
            data.values.nums[i] = 0xffff;
            data.values.ones[i] = 0xffff;
        }
        clearMetadata();
        denormalize(length);
        setSize(length);
        writeMetaData<false>(0xedcba9876543210);
        // writeMetaData<false>(0x0123456789abcde);
        return out;
    }

    template<bool inc>
    inline void incDecOnes(uint_fast8_t idx) {
        ASSERT_CODE_GLOBALE(uint64_t assert_meta = readMetaData<false>();)
        const uint_fast8_t _size = size();
        if constexpr(useBinarySearch) {
            for(; idx < _size - (_size == numChildren); idx += (1 << ctzLookup[idx])) {
                if constexpr(inc) {
                    ++data.values.ones[idx];
                } else {
                    --data.values.ones[idx];
                }
            }
        } else {
            if constexpr(inc) {
                ++data.values.ones[idx];
            } else {
                --data.values.ones[idx];
            }
        }
        ASSERT_E(assert_meta, readMetaData<false>())
    }

    template<bool inc>
    inline void incDecNums(uint_fast8_t idx) {
        ASSERT_CODE_GLOBALE(uint64_t assert_meta = readMetaData<false>();)
        const uint_fast8_t _size = size();
        if constexpr(useBinarySearch) {
            for(; idx < _size - (_size == numChildren); idx += (1 << ctzLookup[idx])) {
                if constexpr(inc) { ++data.values.nums[idx]; } else { --data.values.nums[idx]; }
            }
        } else {
            if constexpr(inc) { ++data.values.nums[idx]; } else { --data.values.nums[idx]; }
        }
        ASSERT_E(assert_meta, readMetaData<false>())
    }


    static void test() {
        {
            const uint64_t meta = 0xfedcba987654321;
            ASSERT_E(NODE2::pInsert(meta, 0), 0xedcba987654321f)
            ASSERT_E(NODE2::pInsert(meta, 1), 0xedcba98765432f1)
            ASSERT_E(NODE2::pInsert(meta, 2), 0xedcba9876543f21)
            ASSERT_E(NODE2::pInsert(meta, 3), 0xedcba987654f321)
            ASSERT_E(NODE2::pInsert(meta, 4), 0xedcba98765f4321)
            ASSERT_E(NODE2::pInsert(meta, 5), 0xedcba9876f54321)
            ASSERT_E(NODE2::pInsert(meta, 6), 0xedcba987f654321)
            ASSERT_E(NODE2::pInsert(meta, 7), 0xedcba98f7654321)
            ASSERT_E(NODE2::pInsert(meta, 8), 0xedcba9f87654321)
            ASSERT_E(NODE2::pInsert(meta, 9), 0xedcbaf987654321)
            ASSERT_E(NODE2::pInsert(meta, 10), 0xedcbfa987654321)
            ASSERT_E(NODE2::pInsert(meta, 11), 0xedcfba987654321)
            ASSERT_E(NODE2::pInsert(meta, 12), 0xedfcba987654321)
            ASSERT_E(NODE2::pInsert(meta, 13), 0xefdcba987654321)
            ASSERT_E(NODE2::pInsert(meta, 14), 0xfedcba987654321)
        }
        //
        {
            const uint64_t meta = 0xfedcba987654321;
            ASSERT_E(NODE2::pRemove(meta, 0), 0x1fedcba98765432)
            ASSERT_E(NODE2::pRemove(meta, 1), 0x2fedcba98765431)
            ASSERT_E(NODE2::pRemove(meta, 2), 0x3fedcba98765421)
            ASSERT_E(NODE2::pRemove(meta, 3), 0x4fedcba98765321)
            ASSERT_E(NODE2::pRemove(meta, 4), 0x5fedcba98764321)
            ASSERT_E(NODE2::pRemove(meta, 5), 0x6fedcba98754321)
            ASSERT_E(NODE2::pRemove(meta, 6), 0x7fedcba98654321)
            ASSERT_E(NODE2::pRemove(meta, 7), 0x8fedcba97654321)
            ASSERT_E(NODE2::pRemove(meta, 8), 0x9fedcba87654321)
            ASSERT_E(NODE2::pRemove(meta, 9), 0xafedcb987654321)
            ASSERT_E(NODE2::pRemove(meta, 10), 0xbfedca987654321)
            ASSERT_E(NODE2::pRemove(meta, 11), 0xcfedba987654321)
            ASSERT_E(NODE2::pRemove(meta, 12), 0xdfecba987654321)
            ASSERT_E(NODE2::pRemove(meta, 13), 0xefdcba987654321)
            ASSERT_E(NODE2::pRemove(meta, 14), 0xfedcba987654321)
        }
        //
        {
            const uint64_t meta = 0xfedcba987654321;
            ASSERT_E(NODE2::rotateRight(meta, 0), 0xfedcba987654321)
            ASSERT_E(NODE2::rotateRight(meta, 1), 0xedcba987654321f)
            ASSERT_E(NODE2::rotateRight(meta, 2), 0xdcba987654321fe)
            ASSERT_E(NODE2::rotateRight(meta, 3), 0xcba987654321fed)
            ASSERT_E(NODE2::rotateRight(meta, 4), 0xba987654321fedc)
            ASSERT_E(NODE2::rotateRight(meta, 5), 0xa987654321fedcb)
            ASSERT_E(NODE2::rotateRight(meta, 6), 0x987654321fedcba)
            ASSERT_E(NODE2::rotateRight(meta, 7), 0x87654321fedcba9)
            ASSERT_E(NODE2::rotateRight(meta, 8), 0x7654321fedcba98)
            ASSERT_E(NODE2::rotateRight(meta, 9), 0x654321fedcba987)
            ASSERT_E(NODE2::rotateRight(meta, 10), 0x54321fedcba9876)
            ASSERT_E(NODE2::rotateRight(meta, 11), 0x4321fedcba98765)
            ASSERT_E(NODE2::rotateRight(meta, 12), 0x321fedcba987654)
            ASSERT_E(NODE2::rotateRight(meta, 13), 0x21fedcba9876543)
            ASSERT_E(NODE2::rotateRight(meta, 14), 0x1fedcba98765432)
        }
        //
        {
            const uint64_t meta = 0xfedcba987654321;
            ASSERT_E(NODE2::rotateLeft(meta, 0), 0xfedcba987654321)
            ASSERT_E(NODE2::rotateLeft(meta, 1), 0x1fedcba98765432)
            ASSERT_E(NODE2::rotateLeft(meta, 2), 0x21fedcba9876543)
            ASSERT_E(NODE2::rotateLeft(meta, 3), 0x321fedcba987654)
            ASSERT_E(NODE2::rotateLeft(meta, 4), 0x4321fedcba98765)
            ASSERT_E(NODE2::rotateLeft(meta, 5), 0x54321fedcba9876)
            ASSERT_E(NODE2::rotateLeft(meta, 6), 0x654321fedcba987)
            ASSERT_E(NODE2::rotateLeft(meta, 7), 0x7654321fedcba98)
            ASSERT_E(NODE2::rotateLeft(meta, 8), 0x87654321fedcba9)
            ASSERT_E(NODE2::rotateLeft(meta, 9), 0x987654321fedcba)
            ASSERT_E(NODE2::rotateLeft(meta, 10), 0xa987654321fedcb)
            ASSERT_E(NODE2::rotateLeft(meta, 11), 0xba987654321fedc)
            ASSERT_E(NODE2::rotateLeft(meta, 12), 0xcba987654321fed)
            ASSERT_E(NODE2::rotateLeft(meta, 13), 0xdcba987654321fe)
            ASSERT_E(NODE2::rotateLeft(meta, 14), 0xedcba987654321f)
        }
    }

};

template<WordSizes wordSize, bool useBinarySearch>
class NODE3 {
public:

    constexpr static const int numChildren = 8;
    constexpr const static std::array<uint8_t, 7> ctzLookup = {0, 1, 0, 2, 0, 1, 0};
    constexpr const static std::array<uint64_t, 8> bitMasks = []()constexpr -> std::array<uint64_t, 8> {
        if constexpr(useBinarySearch) {
            //c=1100,e=1110,f=1111
            //3+3*8=27
            //6+3+2+3+4+3+2+3 = 26
#if __BYTE_ORDER == __LITTLE_ENDIAN
            return {0x3f, 0x8000'0000'c000'0000, 0x0000'0000'c000'0000, 0x8000'0000'c000'0000, 0xc000'0000'c000'0000, 0xc000'0000'8000'0000, 0xc000'0000'0000'0000, 0xc000'0000'8000'0000};
#else
            return {0x3f, 0xc000'0000'8000'0000, 0xc000'0000'0000'0000, 0xc000'0000'8000'0000, 0xc000'0000'c000'0000, 0x8000'0000'c000'0000, 0x0000'0000'c000'0000, 0x8000'0000'c000'0000};
#endif
        } else {
            //6+7*4=34 | 6+7*2=20
            return {0x3f, 0xc000'0000'c000'0000, 0xc000'0000'c000'0000, 0xc000'0000'c000'0000, 0xc000'0000'c000'0000, 0xc000'0000'c000'0000, 0xc000'0000'c000'0000, 0xc000'0000'c000'0000};
        }
    }();

private:
    template<bool sizeOnly>
    inline constexpr bool isMetaCleared() const {
        if(!useBinarySearch) {
            if(!sizeOnly) {
                for(int i = 1; i < 8; ++i) {
                    if((data.rawBytes[i] & bitMasks[i]) != 0)return false;
                }
                return true;
            } else { return (data.rawBytes[0] & bitMasks[0]) == 0; }
        } else {
            if(!sizeOnly) {
                if((data.rawBytes[0] & bitMasks[0] & 0xf) != 0)return false;
                for(int i = 1; i < 8; ++i) {
                    if((data.rawBytes[i] & bitMasks[i]) != 0)return false;
                }
                return true;
            } else {
                return ((data.rawBytes[0] & bitMasks[0] & 0x30) == 0);
            }
        }
    }

    union {
        struct {
            uint8_t *childrenPtr;
            uint32_t nums[numChildren - 1];
            uint32_t ones[numChildren - 1];
        } values;
        uint64_t rawBytes[8];
    } data;

    inline uint_fast8_t getIdx(uint64_t meta, uint_fast8_t idx) const {
        return (meta >> (idx * 3)) & 0b111;
    }

public:

    inline constexpr void clearMetadata() {
        for(int i = 0; i < 8; ++i) {
            data.rawBytes[i] &= ~bitMasks[i];
        }
    }

    inline uint_fast8_t toRealIdx(uint_fast8_t virtualIndex) const {
        return getIdx(readMetaData<false>(), virtualIndex);
    }


private:

    inline void normalize(uint_fast8_t _size) {
        ASSERT_B(isMetaCleared<true>() && isMetaCleared<false>())
        if constexpr(useBinarySearch) {
            uint32_t numsArr[3], onesArr[3];
            _size -= (_size == numChildren);
            for(int i = 0; i < _size; ++i) {
                const uint_fast8_t idx = ctzLookup[i];
                numsArr[idx] = data.values.nums[i];
                onesArr[idx] = data.values.ones[i];
                switch(idx) {
                    case 2:
                        data.values.nums[i] -= numsArr[1];
                        data.values.ones[i] -= onesArr[1]; [[fallthrough]];
                    case 1:
                        data.values.nums[i] -= numsArr[0];
                        data.values.ones[i] -= onesArr[0]; [[fallthrough]];
                    default:
                        (void) 0;
                }
                ASSERT_GE(__builtin_clz(data.values.nums[i]), 2)
                ASSERT_GE(__builtin_clz(data.values.ones[i]), 2)
            }
        } else return;
    }

    inline void denormalize(uint_fast8_t _size) {
        ASSERT_B(isMetaCleared<true>() && isMetaCleared<false>())
        if constexpr(useBinarySearch) {
            _size -= (_size == numChildren);
            uint32_t numsArr[3] = {0}, onesArr[3] = {0};
            for(int i = 0; i < _size; ++i) {
                const uint_fast8_t idx = ctzLookup[i];
                numsArr[idx] = data.values.nums[i];
                onesArr[idx] = data.values.ones[i];
                switch(idx) {
                    case 2:
                        numsArr[idx] += numsArr[1];
                        onesArr[idx] += onesArr[1];[[fallthrough]];
                    case 1:
                        numsArr[idx] += numsArr[0];
                        onesArr[idx] += onesArr[0];[[fallthrough]];
                    default:
                        (void) 0;
                }
                data.values.ones[i] = onesArr[idx];
                data.values.nums[i] = numsArr[idx];
                ASSERT_GE(__builtin_clz(onesArr[idx]), 2 - idx)
                ASSERT_GE(__builtin_clz(numsArr[idx]), 2 - idx)
            }
        } else return;
    }

    static inline constexpr uint64_t rotateRight(uint64_t metaData, uint_fast8_t amount) {
        ASSERT_R(amount, 0, numChildren)
        //24 bit rotation, rotiert in wirklichkeit (kontraintuitiv aber sinnvoll) nach links um 3*amount
        return __builtin_ia32_pext_di(__rolq(metaData << 40, amount * 3), __rolq(0xffff'ff00'0000'0000, amount * 3));
    }

    static inline constexpr uint64_t rotateLeft(uint64_t metaData, uint_fast8_t amount) {
        ASSERT_R(amount, 0, numChildren)
        //24 bit rotation, rotiert in wirklichkeit (kontraintuitiv aber sinnvoll) nach rechts um 3*amount
        return __builtin_ia32_pext_di(__rorq(metaData, amount * 3), __rorq(0x0000'0000'00ff'ffff, amount * 3));

    }

    static inline constexpr uint64_t pInsert(uint64_t metaData, uint_fast8_t idx) {
        ASSERT_R(idx, 0, numChildren)
        //rotiert die bits 24 bis 3*idx nach links.
        return (metaData & ((scu64(1) << (3 * idx)) - 1)) | (__builtin_ia32_pext_di(__rolq(metaData << 40, 3), __rolq(0xffff'ff00'0000'0000 << (3 * idx), 3)) << 3 * idx);
    }

    static inline constexpr uint64_t pRemove(uint64_t metaData, uint_fast8_t idx) {
        ASSERT_R(idx, 0, numChildren)
        //rotiert die bits 24 bis 3*idx nach rechts.
        return (metaData & ((scu64(1) << (3 * idx)) - 1)) | (__builtin_ia32_pext_di(__rorq(metaData >> (3 * idx), 3), __rorq(0x0000'0000'00ff'ffff >> (3 * idx), 3)) << (3 * idx));
    }

    template<bool sizeOnly>
    inline constexpr void writeMetaData(uint64_t metaData) {
        ASSERT_B(isMetaCleared<sizeOnly>())
        if constexpr(useBinarySearch) {
            if constexpr(!sizeOnly) {
                ASSERT_E(metaData & (~(0xff'ffff)), 0)
                //ASSERT_UNREACHEABLE()
                //{0x3f, 0x8000'0000'c000'0000, 0x0000'0000'c000'0000, 0x8000'0000'c000'0000, 0xc000'0000'c000'0000, 0xc000'0000'8000'0000, 0xc000'0000'0000'0000, 0xc000'0000'8000'0000};
                data.rawBytes[0] |= __builtin_ia32_pdep_di(metaData >> 20, bitMasks[0]);//20+4
                data.rawBytes[1] |= __builtin_ia32_pdep_di(metaData >> 17, bitMasks[1]);//17+3
                data.rawBytes[2] |= __builtin_ia32_pdep_di(metaData >> 15, bitMasks[2]);//15+2
                data.rawBytes[3] |= __builtin_ia32_pdep_di(metaData >> 12, bitMasks[3]);//12+3
                data.rawBytes[4] |= __builtin_ia32_pdep_di(metaData >> 8, bitMasks[4]);//8+4
                data.rawBytes[5] |= __builtin_ia32_pdep_di(metaData >> 5, bitMasks[5]);//5+3
                data.rawBytes[6] |= __builtin_ia32_pdep_di(metaData >> 3, bitMasks[6]);//3+2
                data.rawBytes[7] |= __builtin_ia32_pdep_di(metaData, bitMasks[7]);//0+3
            } else {
                ASSERT_E(metaData & (0xffff'ff), 0)
                data.rawBytes[0] |= __builtin_ia32_pdep_di(metaData >> 24, bitMasks[0] & 0b110000);
            }
        } else {
            if constexpr(!sizeOnly) {
                data.rawBytes[2] |= __builtin_ia32_pdep_di(metaData >> 20, bitMasks[2]);//20+4
                data.rawBytes[3] |= __builtin_ia32_pdep_di(metaData >> 16, bitMasks[3]);//16+4
                data.rawBytes[4] |= __builtin_ia32_pdep_di(metaData >> 12, bitMasks[4]);//12+ 4
                data.rawBytes[5] |= __builtin_ia32_pdep_di(metaData >> 8, bitMasks[5]);//8+ 4
                data.rawBytes[6] |= __builtin_ia32_pdep_di(metaData >> 4, bitMasks[6]);//4+ 4
                data.rawBytes[7] |= __builtin_ia32_pdep_di(metaData, bitMasks[7]);//0+ 4
            } else {
                data.rawBytes[0] |= __builtin_ia32_pdep_di(metaData >> 23, bitMasks[0] & 0b111000);
            }
        }
        ASSERT_E(readMetaData<sizeOnly>(), metaData)
    }

    template<bool sizeOnly>
    [[nodiscard]]   inline constexpr uint64_t readMetaData() const {
        if(useBinarySearch) {
            if constexpr(!sizeOnly) {
                return (__builtin_ia32_pext_di(data.rawBytes[0], (bitMasks[0] & 0xf)) << 20) |//20+4
                       (__builtin_ia32_pext_di(data.rawBytes[1], bitMasks[1]) << 17) |//17+3
                       (__builtin_ia32_pext_di(data.rawBytes[2], bitMasks[2]) << 15) |//15+2
                       (__builtin_ia32_pext_di(data.rawBytes[3], bitMasks[3]) << 12) |//12+3
                       (__builtin_ia32_pext_di(data.rawBytes[4], bitMasks[4]) << 8) |//8+4
                       (__builtin_ia32_pext_di(data.rawBytes[5], bitMasks[5]) << 5) |//5+3
                       (__builtin_ia32_pext_di(data.rawBytes[6], bitMasks[6]) << 3) |//3+2
                       (__builtin_ia32_pext_di(data.rawBytes[7], bitMasks[7]));//0+3
            } else {
                // ASSERT_E(__builtin_ia32_pext_di(data.rawBytes[0], bitMasks[0]) & 0b111, 0)
                return (__builtin_ia32_pext_di(data.rawBytes[0], bitMasks[0] & 0b110000)) << 24;
            }
        } else {
            if constexpr(!sizeOnly) {
                return //(__builtin_ia32_pext_di(data.rawBytes[0], bitMasks[0] & 0xf) ) |//56+6
                    //(__builtin_ia32_pext_di(data.rawBytes[1], bitMasks[1]) << 24) |//48+8
                        (__builtin_ia32_pext_di(data.rawBytes[2], bitMasks[2]) << 20) |//40+8
                        (__builtin_ia32_pext_di(data.rawBytes[3], bitMasks[3]) << 16) |//32+8
                        (__builtin_ia32_pext_di(data.rawBytes[4], bitMasks[4]) << 12) |//24+8
                        (__builtin_ia32_pext_di(data.rawBytes[5], bitMasks[5]) << 8) |//16+8
                        (__builtin_ia32_pext_di(data.rawBytes[6], bitMasks[6]) << 4) |//8+8
                        (__builtin_ia32_pext_di(data.rawBytes[7], bitMasks[7]));//0+8
            } else {
                return (__builtin_ia32_pext_di(data.rawBytes[0], bitMasks[0] & 0b111000)) << 20;
            }
        }
    }

    template<bool nums>
    inline uint32_t getSumExceptLast() const {
        if constexpr(useBinarySearch) {
            ASSERT_B(isMetaCleared<true>() || isMetaCleared<false>())
            if constexpr(nums) {
                return data.values.nums[3] + data.values.nums[5] + data.values.nums[6];
            } else {
                return data.values.ones[3] + data.values.ones[5] + data.values.ones[6];
            }
        } else {
            ASSERT_UNIMPLEMETED()
            return 0;
        }
    }


public:
    template<bool tryAlloc, int lowerLevel, bool isLowest>
    inline std::conditional_t<tryAlloc, bool, void> allocate(uint8_t *&lowerChild, uint_fast8_t &idx, uint_fast8_t &lowerIndex, const uint64_t upperFullNums, const uint64_t upperFullOnes, uint64_t &lowerFullNums, uint64_t &lowerFullOnes) {
        static_assert(lowerLevel != 0);
        ASSERT_E(rcuint(&RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(readMetaData<false>(), idx)]), rcuint(lowerChild))
        ASSERT_E(scu32(RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(readMetaData<false>(), idx)].size()), NODE_UN(lowerLevel)::numChildren)
        bool out = true;
        uint64_t meta = readMetaData<false>();
        uint_fast8_t _size = size();
        clearMetadata();
        uint32_t lastNums = upperFullNums - getSumExceptLast<true>();
        uint32_t lastOnes = upperFullOnes - getSumExceptLast<false>();
        normalize(_size);
        const auto realAllocate = [this, &lowerChild, &idx, &lowerIndex, &_size, &meta, &lowerFullNums, &lowerFullOnes, &lastNums, &lastOnes](bool lowest, bool highest) -> bool {
            if(_size == numChildren) {
                if(lowest)--idx;
                else if(highest)++idx;
                if constexpr(!tryAlloc) { ASSERT_UNREACHEABLE("cannot happen") }
                return false;
            }
            ASSERT_R(scu32(idx), 1, numChildren - 2)
            ASSERT_B(!lowest || !highest)
            //uint8_t *childPtr = getChildrenPtr();
            NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
            NODE_UN(lowerLevel) *mChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx)];
            NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)];
            ASSERT_R(lChild->size(), NODE_UN(lowerLevel)::numChildren - 1, NODE_UN(lowerLevel)::numChildren + 1)
            ASSERT_PE(!lowest && !highest, mChild->size(), NODE_UN(lowerLevel)::numChildren)
            ASSERT_R(rChild->size(), NODE_UN(lowerLevel)::numChildren - 1, NODE_UN(lowerLevel)::numChildren + 1)
            lastNums = data.values.nums[numChildren - 2];
            lastOnes = data.values.ones[numChildren - 2];
            for(int i = _size - (_size == numChildren - 1); i > idx; --i) {
                data.values.nums[i] = data.values.nums[i - 1];
                data.values.ones[i] = data.values.ones[i - 1];
            }
            meta = pInsert(meta, idx);
            uint_fast8_t delta1 = (NODE_UN(lowerLevel)::numChildren / 4) + lowest, delta2 = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren, 4), delta3 = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren, 4) - lowest;
            NODE_UN(lowerLevel) *nChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx)];
            data.values.nums[idx] = 0, data.values.ones[idx] = 0;
            uint_fast8_t tIdx = idx - 1, tLIdx = lowerIndex;
            uint8_t *tLChild = lowerChild;
            nChild->template moveInit<uint32_t, isLowest>(lChild, delta1, lChild->size(), tLChild, tIdx, tLIdx, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
            mChild->template moveLeft<uint32_t, isLowest>(nChild, delta2 + delta3, delta1, mChild->size(), data.values.nums[idx], data.values.nums[idx + 1], data.values.ones[idx], data.values.ones[idx + 1]);
            if(idx != numChildren - 3) {
                rChild->template moveLeft<uint32_t, isLowest>(mChild, delta3, mChild->size(), rChild->size(), data.values.nums[idx + 1], data.values.nums[idx + 2], data.values.ones[idx + 1], data.values.ones[idx + 2]);
            } else {
                rChild->template moveLeft<uint32_t, isLowest>(mChild, delta3, mChild->size(), rChild->size(), data.values.nums[idx + 1], lastNums, data.values.ones[idx + 1], lastOnes);
            }
            if(lowest) {
                ASSERT_RI(tIdx, 0, 1)
                idx = tIdx;
                lowerIndex = tLIdx;
                lowerChild = tLChild;
            } else if(highest) {
                if(lowerIndex < delta3) {
                    ++idx;
                    lowerIndex += mChild->size() - (delta3);
                    lowerChild = rcbyte(&RCNODE_2(getChildrenPtr())[getIdx(meta, idx)]);
                } else {
                    lowerIndex -= delta3;
                    idx += 2;
                    ASSERT_E(rcuint(lowerChild), rcuint(&RCNODE_2(getChildrenPtr())[getIdx(meta, idx)]))
                }
            } else {
                if(lowerIndex < delta2 + delta3) {
                    lowerIndex += delta1;
                    lowerChild = rcbyte(&RCNODE_2(getChildrenPtr())[getIdx(meta, idx)]);
                } else {
                    lowerIndex -= (delta2 + delta3);
                    ++idx;
                    ASSERT_E(rcuint(lowerChild), rcuint(&RCNODE_2(getChildrenPtr())[getIdx(meta, idx)]))
                }
            }
            if(idx != NODE_UN(lowerLevel)::numChildren - 1) {
                lowerFullNums = data.values.nums[idx];
                lowerFullOnes = data.values.ones[idx];
            } else {
                lowerFullNums = lastNums;
                lowerFullOnes = lastOnes;
            }
            ++_size;
            return true;
        };
        //einfügen möglich
        if(RANGE(idx, 1, _size - 1)) {
            uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)].size(), rSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)].size();
            NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
            NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)];
            if(idx != numChildren - 2) {
                if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                    uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                    delta -= (lSize < delta);
                    RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint32_t, isLowest>(lChild, delta, lSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
                    if(lowerIndex < delta) {
                        lowerIndex += lSize;
                        --idx;
                        lowerChild = rcbyte(lChild);
                    } else {
                        lowerIndex -= delta;
                    }
                } else if(rSize < NODE_UN(lowerLevel)::numChildren - 1) {
                    const uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - rSize, 2);
                    RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint32_t, isLowest>(rChild, delta, NODE_UN(lowerLevel)::numChildren, rSize, data.values.nums[idx], data.values.nums[idx + 1], data.values.ones[idx], data.values.ones[idx + 1]);
                    if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta) {
                        lowerIndex -= NODE_UN(lowerLevel)::numChildren - delta;
                        ++idx;
                        lowerChild = rcbyte(rChild);
                    }
                } else {
                    out = realAllocate(false, false);
                }
                lowerFullNums = data.values.nums[idx];
                lowerFullOnes = data.values.ones[idx];
            } else {
                if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                    uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                    delta -= (lSize < delta);
                    RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint32_t, isLowest>(lChild, delta, lSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
                    if(lowerIndex < delta) {
                        lowerIndex += lSize;
                        --idx;
                        lowerChild = rcbyte(lChild);
                    } else {
                        lowerIndex -= delta;
                    }
                    lowerFullNums = data.values.nums[idx];
                    lowerFullOnes = data.values.ones[idx];
                } else if(rSize < NODE_UN(lowerLevel)::numChildren - 1) {
                    const uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - rSize, 2);
                    RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint32_t, isLowest>(rChild, delta, NODE_UN(lowerLevel)::numChildren, rSize, data.values.nums[idx], lastNums, data.values.ones[idx], lastOnes);
                    if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta) {
                        lowerIndex -= NODE_UN(lowerLevel)::numChildren - delta;
                        ++idx;
                        lowerChild = rcbyte(rChild);
                        lowerFullNums = lastNums;
                        lowerFullOnes = lastOnes;
                    } else {
                        lowerFullNums = data.values.nums[numChildren - 2];
                        lowerFullOnes = data.values.ones[numChildren - 2];
                    }
                } else {
                    if constexpr(tryAlloc) { out = false; } else { ASSERT_UNREACHEABLE("cannot happen") }
                }
            }
        } else {
            switch(idx) {
                case 0: {
                    if(_size != 2) {
                        NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)];
                        NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 2)];
                        uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)].size(), rSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 2)].size();
                        if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                            const uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                            RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint32_t, isLowest>(lChild, delta, NODE_UN(lowerLevel)::numChildren, lSize, data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                            if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta) {
                                lowerIndex -= NODE_UN(lowerLevel)::numChildren - delta;
                                idx = 1;
                                lowerChild = rcbyte(lChild);
                            }
                        } else if(rSize + lSize < (2 * NODE_UN(lowerLevel)::numChildren) - 1) {
                            uint_fast8_t delta1 = DIV_ROUNDUP((2 * NODE_UN(lowerLevel)::numChildren )- (lSize+rSize), 3),
                                    delta2 = ((2 * NODE_UN(lowerLevel)::numChildren) - (lSize + rSize)) / 3;
                            //delta2 += (rSize == (NODE_UN(lowerLevel)::numChildren - 1));
                            if(idx > NODE_UN(lowerLevel)::numChildren - delta2) {
                                swap(delta1, delta2);
                            }
                            lChild->template moveRight<uint32_t, isLowest>(rChild, delta1 + delta2, lSize, rSize, data.values.nums[1], data.values.nums[2], data.values.ones[1], data.values.ones[2]);
                            RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint32_t, isLowest>(lChild, delta1, NODE_UN(lowerLevel)::numChildren, lSize - (delta1 + delta2), data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                            if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta1) {
                                lowerIndex -= NODE_UN(lowerLevel)::numChildren - delta1;
                                idx = 1;
                                lowerChild = rcbyte(lChild);
                            }
                        } else {
                            ++idx;
                            out = realAllocate(true, false);
                        }
                    } else {
                        uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)].size();
                        if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                            NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)];
                            uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                            delta -= (idx >= NODE_UN(lowerLevel)::numChildren - delta);
                            RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint32_t, isLowest>(lChild, delta, NODE_UN(lowerLevel)::numChildren, lSize, data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                            if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta) {
                                lowerIndex -= NODE_UN(lowerLevel)::numChildren - delta;
                                idx = 1;
                                lowerChild = rcbyte(lChild);
                            }
                        } else {
                            NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)];
                            NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 2)];
                            uint_fast8_t delta1 = DIV_ROUNDUP((2 * NODE_UN(lowerLevel)::numChildren )- (lSize), 3),
                                    delta2 = ((2 * NODE_UN(lowerLevel)::numChildren) - (lSize)) / 3;
                            data.values.nums[2] = 0, data.values.ones[2] = 0;
                            uint8_t *throwaway;
                            uint_fast8_t throwAway2, throwAway3;
                            rChild->template moveInit<uint32_t, isLowest>(lChild, delta1 + delta2, lSize, throwaway, throwAway2, throwAway3, data.values.nums[1], data.values.nums[2], data.values.ones[1], data.values.ones[2]);
                            RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint32_t, isLowest>(lChild, delta1, NODE_UN(lowerLevel)::numChildren, lSize - (delta1 + delta2), data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                            ++_size;
                            if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - (delta1)) {
                                lowerIndex -= NODE_UN(lowerLevel)::numChildren - (delta1);
                                idx = 1;
                                lowerChild = rcbyte(lChild);
                            } else {
                                idx = 0;
                            }
                        }
                    }
                    lowerFullNums = data.values.nums[idx];
                    lowerFullOnes = data.values.ones[idx];
                    break;
                }
                case 1: {
                    uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 0)].size();
                    if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                        NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 0)];
                        uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                        delta -= (lSize < delta);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint32_t, isLowest>(lChild, delta, lSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                        if(lowerIndex < delta) {
                            lowerIndex += lSize;
                            idx = 0;
                            lowerChild = rcbyte(lChild);
                        } else {
                            lowerIndex -= delta;
                        }
                    } else {
                        NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 2)];
                        uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren, 2);
                        delta -= (lSize < delta);
                        data.values.nums[2] = 0, data.values.ones[2] = 0;
                        rChild->template moveInit<uint32_t, isLowest>(RCNODE_UN(lowerLevel)(lowerChild), delta, NODE_UN(lowerLevel)::numChildren, lowerChild, idx, lowerIndex, data.values.nums[1], data.values.nums[2], data.values.ones[1], data.values.ones[2]);
                        ++_size;
                        if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta) {
                            ASSERT_E(scu32(idx), 2)
                            ASSERT_E(rcuint(lowerChild), rcuint(rChild))
                        }
                    }
                    lowerFullNums = data.values.nums[idx];
                    lowerFullOnes = data.values.ones[idx];
                    break;
                }
                case numChildren - 1: {
                    NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 3)];
                    NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 2)];
                    uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 3)].size(), rSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 2)].size();
                    if(rSize < NODE_UN(lowerLevel)::numChildren - 1) {
                        uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - rSize, 2);
                        delta -= (rSize < delta);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint32_t, isLowest>(rChild, delta, rSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[numChildren - 2], lastNums, data.values.ones[numChildren - 2], lastOnes);
                        if(lowerIndex < delta) {
                            lowerIndex += rSize;
                            idx = numChildren - 2;
                            lowerChild = rcbyte(rChild);
                            lowerFullNums = data.values.nums[numChildren - 2];
                            lowerFullOnes = data.values.ones[numChildren - 2];
                        } else {
                            lowerIndex -= delta;
                            lowerFullNums = lastNums;
                            lowerFullOnes = lastOnes;
                        }
                    } else if(lSize + rSize < (2 * NODE_UN(lowerLevel)::numChildren) - 1) {
                        uint_fast8_t delta1 = ((2 * NODE_UN(lowerLevel)::numChildren) - (lSize + rSize)) / 3, delta2 = DIV_ROUNDUP((2 * NODE_UN(lowerLevel)::numChildren )- (lSize+rSize), 3);
                        if(idx < delta1 - 1) {
                            swap(delta1, delta2);
                        }
                        rChild->template moveLeft<uint32_t, isLowest>(lChild, delta1 + delta2, lSize, rSize, data.values.nums[numChildren - 3], data.values.nums[numChildren - 2], data.values.ones[numChildren - 3], data.values.ones[numChildren - 2]);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint32_t, isLowest>(rChild, delta2, rSize - (delta1 + delta2), NODE_UN(lowerLevel)::numChildren, data.values.nums[numChildren - 2], lastNums, data.values.ones[numChildren - 2], lastOnes);
                        if(lowerIndex < delta2) {
                            lowerIndex += rSize - (delta1 + delta2);
                            --idx;
                            lowerChild = rcbyte(rChild);
                            lowerFullNums = data.values.nums[numChildren - 2];
                            lowerFullOnes = data.values.ones[numChildren - 2];
                        } else {
                            lowerIndex -= delta2;
                            lowerFullNums = lastNums;
                            lowerFullOnes = lastOnes;
                        }
                    } else {
                        out = false;
                    }
                    break;
                }
                default: {
                    NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 2)];
                    NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
                    uint_fast8_t lSize = lChild->size(), rSize = rChild->size();
                    if(rSize < NODE_UN(lowerLevel)::numChildren - 1) {
                        uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - rSize, 2);
                        delta -= (lSize < delta);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint32_t, isLowest>(rChild, delta, rSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
                        if(lowerIndex < delta) {
                            lowerIndex += rSize;
                            --idx;
                            lowerChild = rcbyte(rChild);
                        } else {
                            lowerIndex -= delta;
                        }
                    } else if(lSize + rSize < (2 * NODE_UN(lowerLevel)::numChildren) - 1) {
                        uint_fast8_t delta1 = ((2 * NODE_UN(lowerLevel)::numChildren) - (lSize + rSize)) / 3, delta2 = DIV_ROUNDUP((2 * NODE_UN(lowerLevel)::numChildren )- (lSize+rSize), 3);
                        if(idx < delta1 - 1) {
                            swap(delta1, delta2);
                        }
                        rChild->template moveLeft<uint32_t, isLowest>(lChild, delta1 + delta2, lSize, rSize, data.values.nums[idx - 2], data.values.nums[idx - 1], data.values.ones[idx - 2], data.values.ones[idx - 1]);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint32_t, isLowest>(rChild, delta2, rSize - (delta1 + delta2), NODE_UN(lowerLevel)::numChildren, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
                        if(lowerIndex < delta2) {
                            lowerIndex += rSize - (delta1 + delta2);
                            --idx;
                            lowerChild = rcbyte(rChild);
                        } else {
                            lowerIndex -= delta2;
                        }
                    } else {
                        --idx;
                        out = realAllocate(false, true);
                    }
                    lowerFullNums = data.values.nums[idx];
                    lowerFullOnes = data.values.ones[idx];
                    break;
                }
            }
        }
        denormalize(_size);
        writeMetaData<false>(meta);
        setSize(_size);
        if constexpr(tryAlloc) return out;
    }

    template<bool isLowest, int lowerLevel>
    inline bool deallocate(bool isOne, uint64_t fullNums, uint64_t fullOnes, uint64_t curNums, uint64_t curOnes, uint_fast8_t &idx) {
        static_assert(lowerLevel == 2 || lowerLevel == 3);
        ASSERT_E(RCNODE_UN(lowerLevel)(getChildrenPtr())[idx].size(), NODE_UN(lowerLevel)::numChildren)
        uint_fast8_t _size = size();
        bool out = false;
        uint64_t meta = readMetaData<false>();
        clearMetadata();
        uint32_t lastNums = fullNums - getSumExceptLast<true>();
        uint32_t lastOnes = fullOnes - getSumExceptLast<false>();
        normalize(_size);
        const auto merge = [this, &idx, &_size, &meta, &out](NODE_UN(lowerLevel) *lNode, NODE_UN(lowerLevel) *rNode, uint_fast8_t lSize, uint_fast8_t rSize, uint32_t &lNums, uint32_t &lOnes, uint32_t &rNums, uint32_t &rOnes) {
            ASSERT_NE(lSize + rSize, NODE_UN(lowerLevel)::numChildren)
            ASSERT_NE(rcuint(&lNode), rcuint(&rNode))
            rNode->template moveLeft<uint32_t, isLowest>(lNode, rSize, lSize, rSize, lNums, lOnes, rNums, rOnes);
            for(int i = idx + 1; i < _size; ++i) {
                data.values.nums[i] = data.values.nums[i + 1];
                data.values.ones[i] = data.values.ones[i + 1];
            }
            --_size;
            data.values.nums[_size] = 0xffff;
            data.values.ones[_size] = 0xffff;
            meta = pRemove(meta, idx);
            out = true;
        };
        if(idx != numChildren - 1) {
            --data.values.nums[idx];
            data.values.ones[idx] -= isOne;
        }
        switch(idx) {
            case 0: {
                NODE_UN(lowerLevel) *lNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 0)];
                NODE_UN(lowerLevel) *rNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)];
                uint_fast8_t lSize = lNode->size(), rSize = rNode->size();
                if(lSize + rSize <= NODE_UN(lowerLevel)::numChildren) {
                    merge(lNode, rNode, lSize, rSize, data.values.nums[0], data.values.ones[0], data.values.nums[1], data.values.ones[1]);
                }
                break;
            }
            case numChildren - 1: {
                NODE_UN(lowerLevel) *lNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 2)];
                NODE_UN(lowerLevel) *rNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 1)];
                uint_fast8_t lSize = lNode->size(), rSize = rNode->size();
                if(lSize + rSize <= NODE_UN(lowerLevel)::numChildren) {
                    merge(lNode, rNode, lSize, rSize, data.values.nums[numChildren - 2], data.values.ones[numChildren - 2], lastNums, lastOnes);
                    --idx;
                }
                break;
            }
            case numChildren - 2: {
                NODE_UN(lowerLevel) *lNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
                NODE_UN(lowerLevel) *mNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx)];
                NODE_UN(lowerLevel) *rNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)];
                const uint_fast8_t lSize = lNode->size(), mSize = mNode->size(), rSize = rNode->size();
                if(lSize < rSize) {
                    if(lSize + mSize <= NODE_UN(lowerLevel)::numChildren) {
                        merge(lNode, mNode, lSize, mSize, data.values.nums[idx - 1], data.values.ones[idx - 1], data.values.nums[idx], data.values.ones[idx]);
                        --idx;
                    }
                } else {
                    if(mSize + rSize <= NODE_UN(lowerLevel)::numChildren) {
                        merge(mNode, rNode, mSize, rSize, data.values.nums[idx], data.values.ones[idx], lastNums, lastOnes);
                    }
                }
                break;
            }
            default: {
                NODE_UN(lowerLevel) *lNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
                NODE_UN(lowerLevel) *mNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx)];
                NODE_UN(lowerLevel) *rNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)];
                const uint_fast8_t lSize = lNode->size(), mSize = mNode->size(), rSize = rNode->size();
                if(idx != _size - 1) {
                    if(lSize < rSize) {
                        if(lSize + mSize <= NODE_UN(lowerLevel)::numChildren) {
                            merge(lNode, mNode, lSize, mSize, data.values.nums[idx - 1], data.values.ones[idx - 1], data.values.nums[idx], data.values.ones[idx]);
                            --idx;
                        }
                    } else {
                        if(mSize + rSize <= NODE_UN(lowerLevel)::numChildren) {
                            merge(mNode, rNode, mSize, rSize, data.values.nums[idx], data.values.ones[idx], data.values.nums[idx + 1], data.values.ones[idx + 1]);
                        }
                    }
                } else {
                    if(lSize + mSize <= NODE_UN(lowerLevel)::numChildren) {
                        merge(lNode, mNode, lSize, mSize, data.values.nums[idx - 1], data.values.ones[idx - 1], data.values.nums[idx], data.values.ones[idx]);
                        --idx;
                    }
                }
                break;
            }
        }
        clearMetadata();
        denormalize(_size);
        writeMetaData<false>(meta);
        setSize(_size);
        return out;
    }


    template<class intType, bool isLowest>
    inline void moveLeft(NODE3 *lNode, const uint_fast8_t delta, const uint_fast8_t lSize, const uint_fast8_t rSize, intType &lNums, intType &rNums, intType &lOnes, intType &rOnes) {
        static_assert(is_integral_v<intType>);
        static_assert(!is_signed_v<intType>);
        using T = std::conditional_t<isLowest, NODE2, NODE3 >;
        //  ASSERT_B(!isMetaCleared<true>() && !isMetaCleared<false>() && !lNode->isMetaCleared<true>() && !lNode->isMetaCleared<false>())
        const uint64_t lmeta = lNode->template readMetaData<false>(), rmeta = readMetaData<false>();
        ASSERT_LE(lSize + delta, numChildren)
        ASSERT_NE(lNode, this)
        lNode->clearMetadata(), clearMetadata();
        lNode->normalize(lSize), normalize(rSize);
        for(int i = 0; i < delta - 1; ++i) {
            const uint32_t curNums = data.values.nums[i], curOnes = data.values.ones[i];
            lNode->data.values.nums[lSize + i] = curNums;
            lNode->data.values.ones[lSize + i] = curOnes;
            lNums += curNums, lOnes += curOnes;
            rNums -= curNums, rOnes -= curOnes;
            std::memcpy(lNode->getChildrenPtr() + (getIdx(lmeta, lSize + i) * sizeof(T)), getChildrenPtr() + (getIdx(rmeta, i) * sizeof(T)), sizeof(T));
        }
        if(lSize + delta == numChildren) {
            const uint32_t curNums = data.values.nums[delta - 1], curOnes = data.values.ones[delta - 1];
            lNums += curNums, lOnes += curOnes;
            rNums -= curNums, rOnes -= curOnes;
            std::memcpy(lNode->getChildrenPtr() + (getIdx(lmeta, lSize + delta - 1) * sizeof(T)), getChildrenPtr() + (getIdx(rmeta, delta - 1) * sizeof(T)), sizeof(T));
        } else {
            const uint32_t curNums = data.values.nums[delta - 1], curOnes = data.values.ones[delta - 1];
            lNode->data.values.nums[lSize + delta - 1] = curNums;
            lNode->data.values.ones[lSize + delta - 1] = curOnes;
            lNums += curNums, lOnes += curOnes;
            rNums -= curNums, rOnes -= curOnes;
            std::memcpy(lNode->getChildrenPtr() + (getIdx(lmeta, lSize + delta - 1) * sizeof(T)), getChildrenPtr() + (getIdx(rmeta, delta - 1) * sizeof(T)), sizeof(T));

        }
        intType copyFullNums = rNums, copyFullOnes = rOnes;
        for(int i = 0; i < rSize - delta - 1; ++i) {
            const uint32_t curNums = data.values.nums[i + delta], curOnes = data.values.ones[i + delta];
            data.values.nums[i] = curNums;
            data.values.ones[i] = curOnes;
            data.values.nums[i + delta] = 0xffff'ffff;
            data.values.ones[i + delta] = 0xffff'ffff;
            copyFullNums -= curNums;
            copyFullOnes -= curOnes;
        }
        if(rSize != numChildren) {
            data.values.nums[rSize - delta - 1] = data.values.nums[rSize - 1];
            data.values.ones[rSize - delta - 1] = data.values.ones[rSize - 1];
            for(int i = rSize - delta; i < numChildren - 1; ++i) {
                data.values.nums[i] = 0xffff'ffff;
                data.values.ones[i] = 0xffff'ffff;
            }
        } else {
            data.values.nums[rSize - delta - 1] = copyFullNums;
            data.values.ones[rSize - delta - 1] = copyFullOnes;
        }
        ASSERT_CODE_LOCALE(if constexpr(isLowest) {
            for(int i = 0; i < numChildren - 1; ++i) {
                ASSERT_PR(data.values.nums[i] != 0xffff'ffff, data.values.nums[i], wordSize * wordSize / 2, wordSize * wordSize + 1)
            }
        })
        lNode->clearMetadata(), clearMetadata();
        lNode->denormalize(lSize + delta), denormalize(rSize - delta);
        lNode->writeMetaData<false>(lmeta), writeMetaData<false>(rotateLeft(rmeta, delta));
        lNode->setSize(lSize + delta), setSize(rSize - delta);
    }

    template<class intType, bool isLowest>
    inline void moveRight(NODE3 *rNode, const uint_fast8_t delta, const uint_fast8_t lSize, const uint_fast8_t rSize, intType &lNums, intType &rNums, intType &lOnes, intType &rOnes) {
        static_assert(is_integral_v<intType>);
        static_assert(!is_signed_v<intType>);
        using T = std::conditional_t<isLowest, NODE2, NODE3 >;
        //  ASSERT_B(!isMetaCleared<true>() && !isMetaCleared<false>() && !rNode->isMetaCleared<true>() && !rNode->isMetaCleared<false>())
        const uint64_t lmeta = readMetaData<false>(), rmeta = rotateRight(rNode->template readMetaData<false>(), delta);
        ASSERT_LE(rSize + delta, numChildren)
        ASSERT_GE(lSize - delta, numChildren / 2)
        ASSERT_NE(rNode, this)
        rNode->clearMetadata(), clearMetadata();
        rNode->normalize(rSize), normalize(lSize);
        for(int i = numChildren - 2; i >= delta; --i) {
            rNode->data.values.nums[i] = rNode->data.values.nums[i - delta];
            rNode->data.values.ones[i] = rNode->data.values.ones[i - delta];
        }
        for(int i = rSize + delta; i < numChildren - 1; ++i) {
            rNode->data.values.nums[i] = 0xffff'ffff;
            rNode->data.values.ones[i] = 0xffff'ffff;
        }
        for(int i = 0; i < delta - (lSize == numChildren); ++i) {
            const uint32_t curNums = data.values.nums[i + lSize - delta], curOnes = data.values.ones[i + lSize - delta];
            data.values.nums[i + lSize - delta] = 0xffff'ffff;
            data.values.ones[i + lSize - delta] = 0xffff'ffff;
            rNode->data.values.nums[i] = curNums;
            rNode->data.values.ones[i] = curOnes;
            lNums -= curNums, lOnes -= curOnes;
            rNums += curNums, rOnes += curOnes;
            std::memcpy(rNode->getChildrenPtr() + (getIdx(rmeta, i) * sizeof(T)), getChildrenPtr() + (getIdx(lmeta, i + lSize - delta) * sizeof(T)), sizeof(T));
        }
        if(lSize == numChildren) {
            intType copyFullNums = lNums, copyFullOnes = lOnes;
            for(int i = 0; i < lSize - delta; ++i) {
                copyFullNums -= data.values.nums[i];
                copyFullOnes -= data.values.ones[i];
            }
            rNode->data.values.nums[delta - 1] = copyFullNums;
            rNode->data.values.ones[delta - 1] = copyFullOnes;
            lNums -= copyFullNums;
            lOnes -= copyFullOnes;
            rNums += copyFullNums;
            rOnes += copyFullOnes;
        }
        std::memcpy(rNode->getChildrenPtr() + (getIdx(rmeta, delta - 1) * sizeof(T)), getChildrenPtr() + (getIdx(lmeta, lSize - 1) * sizeof(T)), sizeof(T));
        ASSERT_CODE_LOCALE(if constexpr(isLowest) {
            for(int i = 0; i < numChildren - 1; ++i) {
                ASSERT_PR(data.values.nums[i] != 0xffff, data.values.nums[i], wordSize * wordSize / 2, wordSize * wordSize + 1)
            }
            for(int i = 0; i < rSize + delta && i < numChildren - 1; ++i) {
                ASSERT_R(RCNODE_0(rNode->getChildrenPtr())[getIdx(rmeta, i)].template rank<true>(rNode->data.values.nums[i] - (rNode->data.values.nums[i] == wordSize * wordSize)), rNode->data.values.ones[i] - 1, rNode->data.values.ones[i] + 1, i)
            }
            for(int i = 0; i < lSize - delta && i < numChildren - 1; ++i) {
                ASSERT_R(RCNODE_0(getChildrenPtr())[getIdx(lmeta, i)].template rank<true>(data.values.nums[i] - (data.values.nums[i] == wordSize * wordSize)), data.values.ones[i] - 1, data.values.ones[i] + 1, i)
            }
        } else {
            for(int i = 0; i < delta; ++i) {
                uint64_t *rval = rc<uint64_t *>(rNode->getChildrenPtr()) + (8 * getIdx(rmeta, i));
                uint64_t *lval = rc<uint64_t *>(getChildrenPtr()) + (8 * getIdx(lmeta, i + lSize - delta));
                for(int j = 0; j < 8; ++j) {
                    ASSERT_E(rval[j], lval[j], i, j, sizeof(T))
                }
            }
        })
        clearMetadata(), rNode->clearMetadata();
        denormalize(lSize - delta), rNode->denormalize(rSize + delta);
        writeMetaData<false>(lmeta), rNode->writeMetaData<false>(rmeta);
        setSize(lSize - delta), rNode->setSize(rSize + delta);
    }

    inline constexpr void setChildrenPtr(uint8_t *ptr) {
        ASSERT_E(rc<uintptr_t>(ptr) & bitMasks[0], 0)
        data.values.childrenPtr = rcbyte(rcuint(data.values.childrenPtr) & bitMasks[0]);
        data.values.childrenPtr = rcbyte(rcuint(data.values.childrenPtr) | rcuint(ptr));
    }

    [[nodiscard]]   inline constexpr uint8_t *getChildrenPtr() const {
        return rcbyte(rc<uintptr_t>(data.values.childrenPtr) & ~bitMasks[0]);
    }

    [[nodiscard]]  inline constexpr uint_fast8_t size() const {
        if constexpr(useBinarySearch) {
            // printAll((((data.values._nums[4] & 0x3fff'ffff) != 0x3fff'ffff) << 2), readMetaData<true>() >> 24);

            return (readMetaData<true>() >> 24) + (((data.values.nums[3] & 0xffff'ffff) != 0xffff'ffff) << 2) + (((data.values.nums[5] & 0x7fff'ffff) != 0x7fff'ffff));
            // ASSERT_E(out % 2, 1)
            // if(out != 7) {
            //     return 1 + out + ((data.values._nums[out] & (0xffff'ffff >> ctzLookup[out])) != (0xffff'ffff >> ctzLookup[out]));
            // } else {
            //     return numChildren;
            // }
        } else {
            ASSERT_R(readMetaData<true>() >> 23, 0, 8)
            return 1 + (readMetaData<true>() >> 23);
        }
    }

    inline constexpr void setSize(uint_fast8_t newSize) {
        ASSERT_B(isMetaCleared<true>())
        //   ASSERT_R(scu32(newSize), 2, 8 + 1)
        if constexpr(useBinarySearch) {
            if(newSize < 3) {
                data.values.nums[3] = 0xffff'ffff;
            }
            if(newSize < 5) {
                data.values.nums[5] |= 0x7fff'ffff;
            }
            if(newSize > 5) {
                writeMetaData<true>(((newSize - 1) & 0b11) << 24);
            } else {
                writeMetaData<true>(((newSize) & 0b11) << 24);
            }
        } else {
            writeMetaData<true>(scu64(newSize - 1) << 23);
            ASSERT_E(readMetaData<true>() >> 23, scu64((newSize - 1)))
            ASSERT_E(scu32(size()), scu32(newSize))
        }
        ASSERT_E(scu32(size()), scu32(newSize))
    }


    //1,2,3,4,5,6,7
    //0,1,0,2,0,1,0
    inline uint_fast8_t getIdx(uint64_t &key) const {
        //{0,1,0,2,0,1,0,3}
        //{1,2,3,4,5,6,7,8}
        //{0,1,2,3,4,5,6,7}
        // uint64_t meta = readMetaData<false>();
        if constexpr(useBinarySearch) {
            if(key < data.values.nums[3]) {
                //{0,1,2,3}
                if(key < (data.values.nums[1] & 0x7fff'ffff)) {
                    //{0,1}
                    if(key < (data.values.nums[0] & 0x3fff'ffff)) {
                        return 0;
                    } else {
                        key -= (data.values.nums[0] & 0x3fff'ffff);
                        return 1;
                    }
                } else {
                    //{2,3}
                    if((key -= (data.values.nums[1] & 0x7fff'ffff)) < (data.values.nums[2] & 0x3fff'ffff)) {
                        return 2;
                    } else {
                        key -= (data.values.nums[2] & 0x3fff'ffff);
                        return 3;
                    }
                }
            } else {
                //{4,5,6,7}
                if((key -= data.values.nums[3]) < (data.values.nums[5] & 0x7fff'ffff)) {
                    //{4,5}
                    if(key < (data.values.nums[4] & 0x3fff'ffff)) {
                        return 4;
                    } else {
                        key -= (data.values.nums[4] & 0x3fff'ffff);
                        return 5;
                    }
                } else {
                    //{6,7}
                    if((key -= (data.values.nums[5] & 0x7fff'ffff)) < (data.values.nums[6] & 0x3fff'ffff)) {
                        return 6;
                    } else {
                        key -= (data.values.nums[6] & 0x3fff'ffff);
                        return 7;
                    }
                }
            }
        } else {
            uint_fast8_t i = 0;
            uint32_t prevkey = key;
            for(; (key -= (data.values.nums[i] & 0x3fff'ffff)) < prevkey; ++i, prevkey = key);
            key = prevkey;
            return i;
        }
    }

    inline uint_fast8_t getIdx(uint64_t &key, uint64_t &curNums, uint64_t &curOnes) const {
        //{0,1,0,2,0,1,0,3}
        //{1,2,3,4,5,6,7,8}
        //{0,1,2,3,4,5,6,7}
        //uint64_t meta = readMetaData<false>();
        if constexpr(useBinarySearch) {
            if(key <= data.values.nums[3]) {
                //{0,1,2,3}
                if(key <= (data.values.nums[1] & 0x7fff'ffff)) {
                    //{0,1}
                    if(key <= (data.values.nums[0] & 0x3fff'ffff)) {
                        curNums = data.values.nums[0] & 0x3fff'ffff;
                        curOnes = data.values.ones[0] & 0x3fff'ffff;
                        return 0;
                    } else {
                        curNums = (data.values.nums[1] & 0x7fff'ffff) - (data.values.nums[0] & 0x3fff'ffff);
                        curOnes = (data.values.ones[1] & 0x7fff'ffff) - (data.values.ones[0] & 0x3fff'ffff);
                        key -= (data.values.nums[0] & 0x3fff'ffff);
                        return 1;
                    }
                } else {
                    //{2,3}
                    if((key -= (data.values.nums[1] & 0x7fff'ffff)) <= (data.values.nums[2] & 0x3fff'ffff)) {
                        curNums = data.values.nums[2] & 0x3fff'ffff;
                        curOnes = data.values.ones[2] & 0x3fff'ffff;
                        return 2;
                    } else {
                        curNums = (data.values.nums[3]) - (data.values.nums[1] & 0x7fff'ffff) - (data.values.nums[2] & 0x3fff'ffff);
                        curOnes = (data.values.ones[3]) - (data.values.ones[1] & 0x7fff'ffff) - (data.values.ones[2] & 0x3fff'ffff);
                        key -= (data.values.nums[2] & 0x3fff'ffff);
                        return 3;
                    }
                }
            } else {
                //{4,5,6,7}
                if((key -= data.values.nums[3]) <= (data.values.nums[5] & 0x7fff'ffff)) {
                    //{4,5}
                    if(key <= (data.values.nums[4] & 0x3fff'ffff)) {
                        curNums = data.values.nums[4] & 0x3fff'ffff;
                        curOnes = data.values.ones[4] & 0x3fff'ffff;
                        return 4;
                    } else {
                        curNums = (data.values.nums[5] & 0x7fff'ffff) - (data.values.nums[4] & 0x3fff'ffff);
                        curOnes = (data.values.ones[5] & 0x7fff'ffff) - (data.values.ones[4] & 0x3fff'ffff);
                        key -= (data.values.nums[4] & 0x3fff'ffff);
                        return 5;
                    }
                } else {
                    //{6,7}
                    if((key -= (data.values.nums[5] & 0x7fff'ffff)) <= (data.values.nums[6] & 0x3fff'ffff)) {
                        curNums = data.values.nums[6] & 0x3fff'ffff;
                        curOnes = data.values.ones[6] & 0x3fff'ffff;
                        return 6;
                    } else {
                        curNums -= (data.values.nums[3]) + (data.values.nums[5] & 0x7fff'ffff) + (data.values.nums[6] & 0x3fff'ffff);
                        curOnes -= (data.values.ones[3]) + (data.values.ones[5] & 0x7fff'ffff) + (data.values.ones[6] & 0x3fff'ffff);
                        key -= (data.values.nums[6] & 0x3fff'ffff);
                        return 7;
                    }
                }
            }
        } else {
            uint_fast8_t i = 0;
            uint32_t prevkey = key;
            for(; (key -= (data.values.nums[i] & 0x3fff'ffff)) < prevkey; ++i, prevkey = key)
                curNums -= data.values.nums[i] & 0x3fff'ffff;
            key = prevkey;
            return i;
        }
    }

    template<bool rank, bool one>
    inline pair<uint_fast8_t, uint64_t> select_rankIdx(uint64_t &key) const {
        //  uint64_t meta = readMetaData<false>(),
        uint64_t cnums = 0;
        auto arrayFunction = [this](uint_fast8_t idx) constexpr {
            if constexpr(rank) {
                return data.values.nums[idx];
            } else {
                if constexpr(one) {
                    return data.values.ones[idx];
                } else {
                    return data.values.nums[idx] - data.values.ones[idx];
                }
            }
        };
        auto alterFunction = [this](uint_fast8_t idx) constexpr {
            if constexpr(!rank) {
                return data.values.nums[idx];
            } else {
                if constexpr(one) {
                    return data.values.ones[idx];
                } else {
                    return data.values.nums[idx] - data.values.ones[idx];
                }
            }
        };
        uint_fast8_t _size = size();
        if constexpr(useBinarySearch) {
            if((_size <= 4) | (key < arrayFunction(3))) {
                //{0,1,2,3}
                if((_size <= 2) | (key < (arrayFunction(1) & 0x7fff'ffff))) {
                    //{0,1}
                    if((_size <= 1) | (key < (arrayFunction(0) & 0x3fff'ffff))) {
                        return {0, cnums};
                    } else {
                        cnums += (alterFunction(0) & 0x3fff'ffff);
                        key -= (arrayFunction(0) & 0x3fff'ffff);
                        return {1, cnums};
                    }
                } else {
                    //{2,3}
                    cnums += (alterFunction(1) & 0x7fff'ffff);
                    ASSERT_GE(_size, 3)
                    if((key -= (arrayFunction(1) & 0x7fff'ffff)) < (arrayFunction(2) & 0x3fff'ffff)) {
                        return {2, cnums};
                    } else {
                        cnums += (alterFunction(2) & 0x3fff'ffff);
                        key -= (arrayFunction(2) & 0x3fff'ffff);
                        return {3, cnums};
                    }
                }
            } else {
                //{4,5,6,7}
                ASSERT_GE(_size, 5)
                cnums += (alterFunction(3));
                if((_size <= 6) | ((key -= arrayFunction(3)) < (arrayFunction(5) & 0x7fff'ffff))) {
                    //{4,5}
                    if(key < (arrayFunction(4) & 0x3fff'ffff)) {
                        return {4, cnums};
                    } else {
                        cnums += (alterFunction(4) & 0x3fff'ffff);
                        key -= (arrayFunction(4) & 0x3fff'ffff);
                        return {5, cnums};
                    }
                } else {
                    //{6,7}
                    ASSERT_GE(_size, 7)
                    cnums += (alterFunction(5) & 0x7fff'ffff);
                    if((key -= (arrayFunction(5) & 0x7fff'ffff)) < (arrayFunction(6) & 0x3fff'ffff)) {
                        return {6, cnums};
                    } else {
                        cnums += (alterFunction(6) & 0x3fff'ffff);
                        key -= (arrayFunction(6) & 0x3fff'ffff);
                        return {7, cnums};
                    }
                }
            }
        } else {
            uint_fast8_t i = 0;
            uint32_t prevkey = key;
            for(; (key -= (arrayFunction(i) & 0x3fff'ffff)) < prevkey; ++i, prevkey = key)cnums += alterFunction(i) & 0x3fff'ffff;
            key = prevkey;
            return {i, cnums};
        }
    }

    template<class intType, bool isLowest>
    inline void moveInit(NODE3 *lnode, uint_fast8_t delta, uint_fast8_t lSize, uint8_t *&curChild, uint_fast8_t &upperindex, uint_fast8_t &index, intType &lNextNums, intType &rNextNums, intType &lNextOnes, intType &rNextOnes) {
        static_assert(is_integral_v<intType>);
        static_assert(!is_signed_v<intType>);
        ASSERT_R(scu32(lnode->size()), numChildren - 1, numChildren + 1)
        ASSERT_E(rNextNums, 0)
        ASSERT_E(rNextOnes, 0)
        using T = std::conditional_t<isLowest, NODE2, NODE3 >;
        T *leaves;
        if constexpr(isLowest)leaves = DYNAMIC_BV::template allocateNode<3, 2>(); else leaves = DYNAMIC_BV::template allocateNode<3, 3>();
        const uint64_t thisMeta = 07654'3210, lMeta = lnode->readMetaData<false>();
        lnode->clearMetadata();
        lnode->normalize(lSize);
        for(int i = delta; i < numChildren - 1; ++i) {
            data.values.nums[i] = 0xffff'ffff;
            data.values.ones[i] = 0xffff'ffff;
        }
        for(int i = 0; i < delta - 1; ++i) {
            const uint32_t curNums = lnode->data.values.nums[i + lSize - delta], curOnes = lnode->data.values.ones[i + lSize - delta];
            lnode->data.values.nums[i + lSize - delta] = 0xffff'ffff;
            lnode->data.values.ones[i + lSize - delta] = 0xffff'ffff;
            data.values.nums[i] = curNums;
            data.values.ones[i] = curOnes;
            lNextNums -= curNums;
            lNextOnes -= curOnes;
            rNextNums += curNums;
            rNextOnes += curOnes;
            std::memcpy(leaves + (getIdx(thisMeta, i)), lnode->getChildrenPtr() + (getIdx(lMeta, i + lSize - delta) * sizeof(T)), sizeof(T));
        }
        intType copyFullNums = lNextNums, copyFullOnes = lNextOnes;
        for(int i = 0; i < lSize - delta; ++i) {
            copyFullNums -= lnode->data.values.nums[i];
            copyFullOnes -= lnode->data.values.ones[i];
        }
        data.values.nums[delta - 1] = copyFullNums;
        data.values.ones[delta - 1] = copyFullOnes;
        lNextNums -= copyFullNums;
        lNextOnes -= copyFullOnes;
        rNextNums += copyFullNums;
        rNextOnes += copyFullOnes;
        if(index >= lSize - delta) {
            ++upperindex;
            index -= lSize - delta;
            curChild = rcbyte(this);
        }
        std::memcpy(leaves + (getIdx(thisMeta, delta - 1)), lnode->getChildrenPtr() + (getIdx(lMeta, lSize - 1) * sizeof(T)), sizeof(T));
        ASSERT_CODE_LOCALE(if constexpr(isLowest) {
            for(int i = 0; i < delta; ++i) {
                // printAll(RCNODE_0(leaves)[getIdx(thisMeta, i)].template rank<true>(data.values.nums[i] - (data.values.nums[i] == wordSize * wordSize)), data.values.ones[i] - 1, data.values.ones[i] + 1, i);
            }
            for(int i = 0; i < lSize - 1; ++i) {
                ASSERT_PR(data.values.nums[i] != 0xffff, data.values.nums[i], wordSize * wordSize / 2, wordSize * wordSize + 1)
            }
            for(int i = 0; i < delta; ++i) {
                ASSERT_R(RCNODE_0(leaves)[getIdx(thisMeta, i)].template rank<true>(data.values.nums[i] - (data.values.nums[i] == wordSize * wordSize)), sc64(data.values.ones[i]) - 1, data.values.ones[i] + 1, i)
            }
        })
        lnode->clearMetadata();
        lnode->denormalize(lSize - delta);
        lnode->setSize(lSize - delta);
        lnode->template writeMetaData<false>(lMeta);
        clearMetadata();
        denormalize(delta);
        setSize(delta);
        writeMetaData<false>(thisMeta);
        setChildrenPtr(rcbyte(leaves));
    }

    template<uint8_t length>
    inline void init(const uint32_t (&nums)[length], const uint32_t (&ones)[length]) {
        static_assert(length <= numChildren);
        int i = 0;
        for(; i < length; ++i) {
            ASSERT_GE(__builtin_clz(nums[i]), 3)
            ASSERT_GE(__builtin_clz(ones[i]), 3)
            data.values.nums[i] = nums[i];
            data.values.ones[i] = ones[i];
            ASSERT_PLE(useBinarySearch, data.values.nums[i], 0x3fff'ffff)
            ASSERT_PL(!useBinarySearch, data.values.nums[i], 0x3fff'ffff)
            ASSERT_PLE(useBinarySearch, data.values.ones[i], 0x3fff'ffff)
            ASSERT_PL(!useBinarySearch, data.values.ones[i], 0x3fff'ffff)
        }

        for(; i < NODE3::numChildren - 1; ++i) {
            data.values.nums[i] = 0xffff'ffff;
            data.values.ones[i] = 0xffff'ffff;
        }
        clearMetadata();
        denormalize(length);
        setSize(length);
        writeMetaData<false>(07654'3210);
    }

    /**
     *
     * @param begin
     * @param length
     * @return sum over all elements
     */
    template<typename ArrType, typename SizeType>
    inline uint64_t init(NO_ASSERT_CODE(const) SameValueArray<ArrType, SizeType> &numsVec, const uint64_t *onesVec, int begin, int length) {
        ASSERT_B(isMetaCleared<true>() && isMetaCleared<false>())
        ASSERT_R(length, 2, numChildren + 1)
        ASSERT_CODE_GLOBALE(uint32_t numCount = 0, oneCount = 0;)
        int i = 0;
        uint64_t out = 0;
        for(; i < length; ++i, ++begin) {
            out += onesVec[begin];
            data.values.nums[i] = numsVec[begin];
            data.values.ones[i] = onesVec[begin];
            ASSERT_LE(onesVec[begin], numsVec[begin])
        }
        for(; i < numChildren - 1; ++i) {
            data.values.nums[i] = 0xffff'ffff;
            data.values.ones[i] = 0xffff'ffff;
        }
        clearMetadata();
        denormalize(length);
        setSize(length);
        writeMetaData<false>(07654'3210);
        return out;
    }

    template<bool inc>
    inline void incDecOnes(uint_fast8_t idx) {
        ASSERT_CODE_GLOBALE(uint64_t assert_meta = readMetaData<false>();)
        if constexpr(useBinarySearch) {
            const uint_fast8_t _size = size();
            for(; idx < _size - (_size == numChildren); idx += (1 << ctzLookup[idx])) {
                if constexpr(inc) {
                    ++data.values.ones[idx];
                } else {
                    --data.values.ones[idx];
                }
            }
        } else {
            if constexpr(inc) {
                ++data.values.ones[idx];
            } else {
                --data.values.ones[idx];
            }
        }
        ASSERT_E(assert_meta, readMetaData<false>())
    }

    template<bool inc>
    inline void incDecNums(uint_fast8_t idx) {
        ASSERT_CODE_GLOBALE(uint64_t assert_meta = readMetaData<false>();)
        const uint_fast8_t _size = size();
        if constexpr(useBinarySearch) {
            for(; idx < _size - (_size == numChildren); idx += (1 << ctzLookup[idx])) {
                if constexpr(inc) { ++data.values.nums[idx]; } else { --data.values.nums[idx]; }
            }
        } else {
            if constexpr(inc) { ++data.values.nums[idx]; } else { --data.values.nums[idx]; }
        }
        ASSERT_E(assert_meta, readMetaData<false>())
    }


    static void test() {
        {
            const uint64_t meta = 076543210;
            ASSERT_E(NODE3::pInsert(meta, 0), 065432107)
            ASSERT_E(NODE3::pInsert(meta, 1), 065432170)
            ASSERT_E(NODE3::pInsert(meta, 2), 065432710)
            ASSERT_E(NODE3::pInsert(meta, 3), 065437210)
            ASSERT_E(NODE3::pInsert(meta, 4), 065473210)
            ASSERT_E(NODE3::pInsert(meta, 5), 065743210)
            ASSERT_E(NODE3::pInsert(meta, 6), 067543210)
            ASSERT_E(NODE3::pInsert(meta, 7), 076543210)
        }
        //
        {
            const uint64_t meta = 076543210;
            ASSERT_E(NODE3::pRemove(meta, 0), 007654321)
            ASSERT_E(NODE3::pRemove(meta, 1), 017654320)
            ASSERT_E(NODE3::pRemove(meta, 2), 027654310)
            ASSERT_E(NODE3::pRemove(meta, 3), 037654210)
            ASSERT_E(NODE3::pRemove(meta, 4), 047653210)
            ASSERT_E(NODE3::pRemove(meta, 5), 057643210)
            ASSERT_E(NODE3::pRemove(meta, 6), 067543210)
            ASSERT_E(NODE3::pRemove(meta, 7), 076543210)
        }
        //
        {
            const uint64_t meta = 076543210;
            ASSERT_E(NODE3::rotateLeft(meta, 0), 076543210)
            ASSERT_E(NODE3::rotateLeft(meta, 1), 007654321)
            ASSERT_E(NODE3::rotateLeft(meta, 2), 010765432)
            ASSERT_E(NODE3::rotateLeft(meta, 3), 021076543)
            ASSERT_E(NODE3::rotateLeft(meta, 4), 032107654)
            ASSERT_E(NODE3::rotateLeft(meta, 5), 043210765)
            ASSERT_E(NODE3::rotateLeft(meta, 6), 054321076)
            ASSERT_E(NODE3::rotateLeft(meta, 7), 065432107)
        }
        //
        {
            const uint64_t meta = 076543210;
            ASSERT_E(NODE3::rotateRight(meta, 0), 076543210)
            ASSERT_E(NODE3::rotateRight(meta, 1), 065432107)
            ASSERT_E(NODE3::rotateRight(meta, 2), 054321076)
            ASSERT_E(NODE3::rotateRight(meta, 3), 043210765)
            ASSERT_E(NODE3::rotateRight(meta, 4), 032107654)
            ASSERT_E(NODE3::rotateRight(meta, 5), 021076543)
            ASSERT_E(NODE3::rotateRight(meta, 6), 010765432)
            ASSERT_E(NODE3::rotateRight(meta, 7), 007654321)
        }
    }
};

template<WordSizes wordSize, bool useBinarySearch>
class NODE4 {
public:

    constexpr static const int numChildren = 4;
private:


    static inline constexpr uint64_t rotateRight(uint64_t metaData, uint_fast8_t amount) {
        ASSERT_R(amount, 0, numChildren)
        return __rolw(metaData, amount * 4);
        //    return __builtin_ia32_pext_si(__rold(metaData << 16, amount * 4), __rold(0xffff'0000, amount * 4));
    }

    static inline constexpr uint64_t rotateLeft(uint64_t metaData, uint_fast8_t amount) {
        ASSERT_R(amount, 0, numChildren)
        return __rorw(metaData, amount * 4);
        //  return __builtin_ia32_pext_si(__rord(metaData, amount * 4), __rord(0x0000'ffff, amount * 4));

    }

    static inline constexpr uint64_t pInsert(uint64_t metaData, uint_fast8_t idx) {
        return (metaData & ((scu32(1) << (4 * idx)) - 1)) | (__builtin_ia32_pext_di(__rolq(metaData << 48, 4), __rolq(0xffff'0000'0000'0000 << (4 * idx), 4)) << 4 * idx);
    }

    static inline constexpr uint64_t pRemove(uint64_t metaData, uint_fast8_t idx) {
        return (metaData & ((scu32(1) << (4 * idx)) - 1)) | (__builtin_ia32_pext_di(__rorq(metaData >> (4 * idx), 4), __rorq(0x0000'0000'0000'ffff >> (4 * idx), 4)) << (4 * idx));
    }

    template<bool sizeOnly>
    inline constexpr bool isMetaCleared() const {
        return true;
    }

    template<bool sizeOnly>
    inline constexpr void writeMetaData(uint64_t metaData) {
        if constexpr(!sizeOnly) {
            data.values.rest |= metaData;
        } else {
            data.values.rest |= metaData;
        }
        ASSERT_E(readMetaData<sizeOnly>(), metaData)
    }

    template<bool sizeOnly>
    [[nodiscard]]   inline constexpr uint64_t readMetaData() const {
        if constexpr(!sizeOnly) {
            return data.values.rest & 0xffff;
        } else {
            return data.values.rest & 0xf'0000;
        }
    }

    inline uint_fast8_t getIdx(uint64_t meta, uint_fast8_t idx) const {
        return (meta >> (4 * idx)) & 0xf;
    }


    union {
        struct {
            uint8_t *childrenPtr;
            uint64_t nums[numChildren - 1];
            uint64_t ones[numChildren - 1];
            uint64_t rest;//= 0x3210;
        } values;
        uint64_t rawBytes[8];
    } data;
    constexpr const static std::array<uint8_t, 3> ctzLookup = {0, 1, 0};

    inline void normalize(uint_fast8_t _size) {
        if constexpr(useBinarySearch) {
            _size -= (_size == numChildren);
            clearMetadata();
            uint64_t numsArr[3], onesArr[3];
            for(int i = 0; i < _size; ++i) {
                const uint_fast8_t idx = ctzLookup[i];
                numsArr[idx] = data.values.nums[i];
                onesArr[idx] = data.values.ones[i];
                if(idx == 1) {
                    data.values.nums[i] -= numsArr[0];
                    data.values.ones[i] -= onesArr[0];
                }
                ASSERT_GE(__builtin_clzll(data.values.nums[i]), 1)
                ASSERT_GE(__builtin_clzll(data.values.ones[i]), 1)
            }
        } else return;
    }

    inline void denormalize(uint_fast8_t _size) {
        if constexpr(useBinarySearch) {
            uint64_t numsArr[2], onesArr[2];
            for(int i = 0; i < _size; ++i) {
                _size -= (_size == numChildren);
                const uint_fast8_t idx = ctzLookup[i];
                numsArr[idx] = data.values.nums[i];
                onesArr[idx] = data.values.ones[i];
                if(idx == 1) {
                    numsArr[idx] += numsArr[0];
                    onesArr[idx] += onesArr[0];
                }
                data.values.ones[i] = onesArr[idx];
                data.values.nums[i] = numsArr[idx];
                ASSERT_GE(__builtin_clzll(data.values.ones[i]), 1 - idx)
                ASSERT_GE(__builtin_clzll(data.values.nums[i]), 1 - idx)
            }
        } else return;
    }

    template<bool nums>
    inline uint64_t getSumExceptLast() const {
        if constexpr(useBinarySearch) {
            ASSERT_B(isMetaCleared<true>() || isMetaCleared<false>())
            if constexpr(nums) {
                return data.values.nums[3];
            } else {
                return data.values.ones[3];
            }
        } else {
            ASSERT_UNIMPLEMETED()
            return 0;
        }
    }

public:
    inline uint_fast8_t toRealIdx(uint_fast8_t virtualIndex) const {
        return getIdx(readMetaData<false>(), virtualIndex);
    }

    inline constexpr void clearMetadata() {
        data.values.rest = 0;
    }

    inline constexpr uint8_t *getChildrenPtr() const {
        return data.values.childrenPtr;
    }

    inline constexpr void setChildrenPtr(uint8_t *ptr) {
        data.values.childrenPtr = ptr;
    }

    [[nodiscard]] inline constexpr uint_fast8_t size() const {
        ASSERT_R(readMetaData<true>() >> 16, 0, 5)
        return (readMetaData<true>() >> 16);
    }

    inline constexpr void setSize(uint_fast8_t newSize) {
        ASSERT_R(newSize, 2, 5)
//        ASSERT_R(newSize, DIV_ROUNDUP(numChildren, 2), numChildren)
        writeMetaData<true>(scu64(newSize) << 16);
        ASSERT_E(readMetaData<true>() >> 16, scu64((newSize)))
        ASSERT_E(scu32(size()), scu32(newSize))
    }

    template<bool tryAlloc, int lowerLevel, bool isLowest>
    inline std::conditional_t<tryAlloc, bool, void> allocate(uint8_t *&lowerChild, uint_fast8_t &idx, uint_fast8_t &lowerIndex, const uint64_t upperFullNums, const uint64_t upperFullOnes, uint64_t &lowerFullNums, uint64_t &lowerFullOnes) {
        static_assert(lowerLevel != 0);
        ASSERT_E(rcuint(&RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(readMetaData<false>(), idx)]), rcuint(lowerChild))
        ASSERT_E(scu32(RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(readMetaData<false>(), idx)].size()), NODE_UN(lowerLevel)::numChildren)
        bool out = true;
        uint64_t meta = readMetaData<false>();
        uint_fast8_t _size = size();
        clearMetadata();
        uint64_t lastNums = upperFullNums - getSumExceptLast<true>();
        uint64_t lastOnes = upperFullOnes - getSumExceptLast<false>();
        normalize(_size);
        const auto realAllocate = [this, &lowerChild, &idx, &lowerIndex, &_size, &meta, &lowerFullNums, &lowerFullOnes, &lastNums, &lastOnes](bool lowest, bool highest) -> bool {
            if(_size == numChildren) {
                if(lowest)--idx;
                else if(highest)++idx;
                if constexpr(!tryAlloc) { ASSERT_UNREACHEABLE("cannot happen") }
                return false;
            }
            ASSERT_R(scu32(idx), 1, numChildren - 2)
            ASSERT_B(!lowest || !highest)
            //uint8_t *childPtr = getChildrenPtr();
            NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
            NODE_UN(lowerLevel) *mChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx)];
            NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)];
            ASSERT_R(lChild->size(), NODE_UN(lowerLevel)::numChildren - 1, NODE_UN(lowerLevel)::numChildren + 1)
            ASSERT_PE(!lowest && !highest, mChild->size(), NODE_UN(lowerLevel)::numChildren)
            ASSERT_R(rChild->size(), NODE_UN(lowerLevel)::numChildren - 1, NODE_UN(lowerLevel)::numChildren + 1)
            lastNums = data.values.nums[numChildren - 2];
            lastOnes = data.values.ones[numChildren - 2];
            for(int i = _size - (_size == numChildren - 1); i > idx; --i) {
                data.values.nums[i] = data.values.nums[i - 1];
                data.values.ones[i] = data.values.ones[i - 1];
            }
            meta = pInsert(meta, idx);
            uint_fast8_t delta1 = (NODE_UN(lowerLevel)::numChildren / 4) + lowest, delta2 = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren, 4), delta3 = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren, 4) - lowest;
            NODE_UN(lowerLevel) *nChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx)];
            data.values.nums[idx] = 0, data.values.ones[idx] = 0;
            uint_fast8_t tIdx = idx - 1, tLIdx = lowerIndex;
            uint8_t *tLChild = lowerChild;
            nChild->template moveInit<uint64_t, isLowest>(lChild, delta1, lChild->size(), tLChild, tIdx, tLIdx, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
            mChild->template moveLeft<uint64_t, isLowest>(nChild, delta2 + delta3, delta1, mChild->size(), data.values.nums[idx], data.values.nums[idx + 1], data.values.ones[idx], data.values.ones[idx + 1]);
            if(idx != numChildren - 3) {
                rChild->template moveLeft<uint64_t, isLowest>(mChild, delta3, mChild->size(), rChild->size(), data.values.nums[idx + 1], data.values.nums[idx + 2], data.values.ones[idx + 1], data.values.ones[idx + 2]);
            } else {
                rChild->template moveLeft<uint64_t, isLowest>(mChild, delta3, mChild->size(), rChild->size(), data.values.nums[idx + 1], lastNums, data.values.ones[idx + 1], lastOnes);
            }
            if(lowest) {
                ASSERT_RI(tIdx, 0, 1)
                idx = tIdx;
                lowerIndex = tLIdx;
                lowerChild = tLChild;
            } else if(highest) {
                if(lowerIndex < delta3) {
                    ++idx;
                    lowerIndex += mChild->size() - (delta3);
                    lowerChild = rcbyte(&RCNODE_2(getChildrenPtr())[getIdx(meta, idx)]);
                } else {
                    lowerIndex -= delta3;
                    idx += 2;
                    ASSERT_E(rcuint(lowerChild), rcuint(&RCNODE_2(getChildrenPtr())[getIdx(meta, idx)]))
                }
            } else {
                if(lowerIndex < delta2 + delta3) {
                    lowerIndex += delta1;
                    lowerChild = rcbyte(&RCNODE_2(getChildrenPtr())[getIdx(meta, idx)]);
                } else {
                    lowerIndex -= (delta2 + delta3);
                    ++idx;
                    ASSERT_E(rcuint(lowerChild), rcuint(&RCNODE_2(getChildrenPtr())[getIdx(meta, idx)]))
                }
            }
            if(idx != NODE_UN(lowerLevel)::numChildren - 1) {
                lowerFullNums = data.values.nums[idx];
                lowerFullOnes = data.values.ones[idx];
            } else {
                lowerFullNums = lastNums;
                lowerFullOnes = lastOnes;
            }
            ++_size;
            return true;
        };
        //einfügen möglich
        if(RANGE(idx, 1, _size - 1)) {
            uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)].size(), rSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)].size();
            NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
            NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)];
            if(idx != numChildren - 2) {
                if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                    uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                    delta -= (lSize < delta);
                    RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint64_t, isLowest>(lChild, delta, lSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
                    if(lowerIndex < delta) {
                        lowerIndex += lSize;
                        --idx;
                        lowerChild = rcbyte(lChild);
                    } else {
                        lowerIndex -= delta;
                    }
                } else if(rSize < NODE_UN(lowerLevel)::numChildren - 1) {
                    const uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - rSize, 2);
                    RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint64_t, isLowest>(rChild, delta, NODE_UN(lowerLevel)::numChildren, rSize, data.values.nums[idx], data.values.nums[idx + 1], data.values.ones[idx], data.values.ones[idx + 1]);
                    if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta) {
                        lowerIndex -= NODE_UN(lowerLevel)::numChildren - delta;
                        ++idx;
                        lowerChild = rcbyte(rChild);
                    }
                } else {
                    out = realAllocate(false, false);
                }
                lowerFullNums = data.values.nums[idx];
                lowerFullOnes = data.values.ones[idx];
            } else {
                if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                    uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                    delta -= (lSize < delta);
                    RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint64_t, isLowest>(lChild, delta, lSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
                    if(lowerIndex < delta) {
                        lowerIndex += lSize;
                        --idx;
                        lowerChild = rcbyte(lChild);
                    } else {
                        lowerIndex -= delta;
                    }
                    lowerFullNums = data.values.nums[idx];
                    lowerFullOnes = data.values.ones[idx];
                } else if(rSize < NODE_UN(lowerLevel)::numChildren - 1) {
                    const uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - rSize, 2);
                    RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint64_t, isLowest>(rChild, delta, NODE_UN(lowerLevel)::numChildren, rSize, data.values.nums[idx], lastNums, data.values.ones[idx], lastOnes);
                    if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta) {
                        lowerIndex -= NODE_UN(lowerLevel)::numChildren - delta;
                        ++idx;
                        lowerChild = rcbyte(rChild);
                        lowerFullNums = lastNums;
                        lowerFullOnes = lastOnes;
                    } else {
                        lowerFullNums = data.values.nums[numChildren - 2];
                        lowerFullOnes = data.values.ones[numChildren - 2];
                    }
                } else {
                    if constexpr(tryAlloc) { out = false; } else { ASSERT_UNREACHEABLE("cannot happen") }
                }
            }
        } else {
            switch(idx) {
                case 0: {
                    if(_size != 2) {
                        NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)];
                        NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 2)];
                        uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)].size(), rSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 2)].size();
                        if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                            const uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                            RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint64_t, isLowest>(lChild, delta, NODE_UN(lowerLevel)::numChildren, lSize, data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                            if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta) {
                                lowerIndex -= NODE_UN(lowerLevel)::numChildren - delta;
                                idx = 1;
                                lowerChild = rcbyte(lChild);
                            }
                        } else if(rSize + lSize < (2 * NODE_UN(lowerLevel)::numChildren) - 1) {
                            uint_fast8_t delta1 = DIV_ROUNDUP((2 * NODE_UN(lowerLevel)::numChildren )- (lSize+rSize), 3),
                                    delta2 = ((2 * NODE_UN(lowerLevel)::numChildren) - (lSize + rSize)) / 3;
                            //delta2 += (rSize == (NODE_UN(lowerLevel)::numChildren - 1));
                            if(idx > NODE_UN(lowerLevel)::numChildren - delta2) {
                                swap(delta1, delta2);
                            }
                            lChild->template moveRight<uint64_t, isLowest>(rChild, delta1 + delta2, lSize, rSize, data.values.nums[1], data.values.nums[2], data.values.ones[1], data.values.ones[2]);
                            RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint64_t, isLowest>(lChild, delta1, NODE_UN(lowerLevel)::numChildren, lSize - (delta1 + delta2), data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                            if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta1) {
                                lowerIndex -= NODE_UN(lowerLevel)::numChildren - delta1;
                                idx = 1;
                                lowerChild = rcbyte(lChild);
                            }
                        } else {
                            ++idx;
                            out = realAllocate(true, false);
                        }
                    } else {
                        uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)].size();
                        if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                            NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)];
                            uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                            delta -= (idx >= NODE_UN(lowerLevel)::numChildren - delta);
                            RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint64_t, isLowest>(lChild, delta, NODE_UN(lowerLevel)::numChildren, lSize, data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                            if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta) {
                                lowerIndex -= NODE_UN(lowerLevel)::numChildren - delta;
                                idx = 1;
                                lowerChild = rcbyte(lChild);
                            }
                        } else {
                            NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)];
                            NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 2)];
                            uint_fast8_t delta1 = DIV_ROUNDUP((2 * NODE_UN(lowerLevel)::numChildren )- (lSize), 3),
                                    delta2 = ((2 * NODE_UN(lowerLevel)::numChildren) - (lSize)) / 3;
                            data.values.nums[2] = 0, data.values.ones[2] = 0;
                            uint8_t *throwaway;
                            uint_fast8_t throwAway2, throwAway3;
                            rChild->template moveInit<uint64_t, isLowest>(lChild, delta1 + delta2, lSize, throwaway, throwAway2, throwAway3, data.values.nums[1], data.values.nums[2], data.values.ones[1], data.values.ones[2]);
                            RCNODE_UN(lowerLevel)(lowerChild)->template moveRight<uint64_t, isLowest>(lChild, delta1, NODE_UN(lowerLevel)::numChildren, lSize - (delta1 + delta2), data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                            ++_size;
                            if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - (delta1)) {
                                lowerIndex -= NODE_UN(lowerLevel)::numChildren - (delta1);
                                idx = 1;
                                lowerChild = rcbyte(lChild);
                            } else {
                                idx = 0;
                            }
                        }
                    }
                    lowerFullNums = data.values.nums[idx];
                    lowerFullOnes = data.values.ones[idx];
                    break;
                }
                case 1: {
                    uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 0)].size();
                    if(lSize < NODE_UN(lowerLevel)::numChildren - 1) {
                        NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 0)];
                        uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - lSize, 2);
                        delta -= (lSize < delta);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint64_t, isLowest>(lChild, delta, lSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[0], data.values.nums[1], data.values.ones[0], data.values.ones[1]);
                        if(lowerIndex < delta) {
                            lowerIndex += lSize;
                            idx = 0;
                            lowerChild = rcbyte(lChild);
                        } else {
                            lowerIndex -= delta;
                        }
                    } else {
                        NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 2)];
                        uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren, 2);
                        delta -= (lSize < delta);
                        data.values.nums[2] = 0, data.values.ones[2] = 0;
                        rChild->template moveInit<uint64_t, isLowest>(RCNODE_UN(lowerLevel)(lowerChild), delta, NODE_UN(lowerLevel)::numChildren, lowerChild, idx, lowerIndex, data.values.nums[1], data.values.nums[2], data.values.ones[1], data.values.ones[2]);
                        ++_size;
                        if(lowerIndex >= NODE_UN(lowerLevel)::numChildren - delta) {
                            ASSERT_E(scu32(idx), 2)
                            ASSERT_E(rcuint(lowerChild), rcuint(rChild))
                        }
                    }
                    lowerFullNums = data.values.nums[idx];
                    lowerFullOnes = data.values.ones[idx];
                    break;
                }
                case numChildren - 1: {
                    NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 3)];
                    NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 2)];
                    uint_fast8_t lSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 3)].size(), rSize = RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 2)].size();
                    if(rSize < NODE_UN(lowerLevel)::numChildren - 1) {
                        uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - rSize, 2);
                        delta -= (rSize < delta);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint64_t, isLowest>(rChild, delta, rSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[numChildren - 2], lastNums, data.values.ones[numChildren - 2], lastOnes);
                        if(lowerIndex < delta) {
                            lowerIndex += rSize;
                            idx = numChildren - 2;
                            lowerChild = rcbyte(rChild);
                            lowerFullNums = data.values.nums[numChildren - 2];
                            lowerFullOnes = data.values.ones[numChildren - 2];
                        } else {
                            lowerIndex -= delta;
                            lowerFullNums = lastNums;
                            lowerFullOnes = lastOnes;
                        }
                    } else if(lSize + rSize < (2 * NODE_UN(lowerLevel)::numChildren) - 1) {
                        uint_fast8_t delta1 = ((2 * NODE_UN(lowerLevel)::numChildren) - (lSize + rSize)) / 3, delta2 = DIV_ROUNDUP((2 * NODE_UN(lowerLevel)::numChildren )- (lSize+rSize), 3);
                        if(idx < delta1 - 1) {
                            swap(delta1, delta2);
                        }
                        rChild->template moveLeft<uint64_t, isLowest>(lChild, delta1 + delta2, lSize, rSize, data.values.nums[numChildren - 3], data.values.nums[numChildren - 2], data.values.ones[numChildren - 3], data.values.ones[numChildren - 2]);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint64_t, isLowest>(rChild, delta2, rSize - (delta1 + delta2), NODE_UN(lowerLevel)::numChildren, data.values.nums[numChildren - 2], lastNums, data.values.ones[numChildren - 2], lastOnes);
                        if(lowerIndex < delta2) {
                            lowerIndex += rSize - (delta1 + delta2);
                            --idx;
                            lowerChild = rcbyte(rChild);
                            lowerFullNums = data.values.nums[numChildren - 2];
                            lowerFullOnes = data.values.ones[numChildren - 2];
                        } else {
                            lowerIndex -= delta2;
                            lowerFullNums = lastNums;
                            lowerFullOnes = lastOnes;
                        }
                    } else {
                        out = false;
                    }
                    break;
                }
                default: {
                    NODE_UN(lowerLevel) *lChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 2)];
                    NODE_UN(lowerLevel) *rChild = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
                    uint_fast8_t lSize = lChild->size(), rSize = rChild->size();
                    if(rSize < NODE_UN(lowerLevel)::numChildren - 1) {
                        uint_fast8_t delta = DIV_ROUNDUP(NODE_UN(lowerLevel)::numChildren - rSize, 2);
                        delta -= (lSize < delta);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint64_t, isLowest>(rChild, delta, rSize, NODE_UN(lowerLevel)::numChildren, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
                        if(lowerIndex < delta) {
                            lowerIndex += rSize;
                            --idx;
                            lowerChild = rcbyte(rChild);
                        } else {
                            lowerIndex -= delta;
                        }
                    } else if(lSize + rSize < (2 * NODE_UN(lowerLevel)::numChildren) - 1) {
                        uint_fast8_t delta1 = ((2 * NODE_UN(lowerLevel)::numChildren) - (lSize + rSize)) / 3, delta2 = DIV_ROUNDUP((2 * NODE_UN(lowerLevel)::numChildren )- (lSize+rSize), 3);
                        if(idx < delta1 - 1) {
                            swap(delta1, delta2);
                        }
                        rChild->template moveLeft<uint64_t, isLowest>(lChild, delta1 + delta2, lSize, rSize, data.values.nums[idx - 2], data.values.nums[idx - 1], data.values.ones[idx - 2], data.values.ones[idx - 1]);
                        RCNODE_UN(lowerLevel)(lowerChild)->template moveLeft<uint64_t, isLowest>(rChild, delta2, rSize - (delta1 + delta2), NODE_UN(lowerLevel)::numChildren, data.values.nums[idx - 1], data.values.nums[idx], data.values.ones[idx - 1], data.values.ones[idx]);
                        if(lowerIndex < delta2) {
                            lowerIndex += rSize - (delta1 + delta2);
                            --idx;
                            lowerChild = rcbyte(rChild);
                        } else {
                            lowerIndex -= delta2;
                        }
                    } else {
                        --idx;
                        out = realAllocate(false, true);
                    }
                    lowerFullNums = data.values.nums[idx];
                    lowerFullOnes = data.values.ones[idx];
                    break;
                }
            }
        }
        denormalize(_size);
        writeMetaData<false>(meta);
        setSize(_size);
        if constexpr(tryAlloc) return out;
    }

    template<bool isLowest, int lowerLevel>
    inline bool deallocate(bool isOne, uint64_t fullNums, uint64_t fullOnes, uint64_t curNums, uint64_t curOnes, uint_fast8_t &idx) {
        static_assert(lowerLevel == 3 || lowerLevel == 4);
        ASSERT_E(RCNODE_UN(lowerLevel)(getChildrenPtr())[idx].size(), NODE_UN(lowerLevel)::numChildren)
        uint_fast8_t _size = size();
        bool out = false;
        uint64_t meta = readMetaData<false>();
        clearMetadata();
        uint64_t lastNums = fullNums - getSumExceptLast<true>();
        uint64_t lastOnes = fullOnes - getSumExceptLast<false>();
        normalize(_size);
        const auto merge = [this, &idx, &_size, &meta, &out](NODE_UN(lowerLevel) *lNode, NODE_UN(lowerLevel) *rNode, uint_fast8_t lSize, uint_fast8_t rSize, uint64_t &lNums, uint64_t &lOnes, uint64_t &rNums, uint64_t &rOnes) {
            ASSERT_NE(lSize + rSize, NODE_UN(lowerLevel)::numChildren)
            ASSERT_NE(rcuint(&lNode), rcuint(&rNode))
            rNode->template moveLeft<uint64_t, isLowest>(lNode, rSize, lSize, rSize, lNums, lOnes, rNums, rOnes);
            for(int i = idx + 1; i < _size; ++i) {
                data.values.nums[i] = data.values.nums[i + 1];
                data.values.ones[i] = data.values.ones[i + 1];
            }
            --_size;
            data.values.nums[_size] = 0xffff;
            data.values.ones[_size] = 0xffff;
            meta = pRemove(meta, idx);
            out = true;
        };
        if(idx != numChildren - 1) {
            --data.values.nums[idx];
            data.values.ones[idx] -= isOne;
        }
        switch(idx) {
            case 0: {
                NODE_UN(lowerLevel) *lNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 0)];
                NODE_UN(lowerLevel) *rNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, 1)];
                uint_fast8_t lSize = lNode->size(), rSize = rNode->size();
                if(lSize + rSize <= NODE_UN(lowerLevel)::numChildren) {
                    merge(lNode, rNode, lSize, rSize, data.values.nums[0], data.values.ones[0], data.values.nums[1], data.values.ones[1]);
                }
                break;
            }
            case numChildren - 1: {
                NODE_UN(lowerLevel) *lNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 2)];
                NODE_UN(lowerLevel) *rNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, numChildren - 1)];
                uint_fast8_t lSize = lNode->size(), rSize = rNode->size();
                if(lSize + rSize <= NODE_UN(lowerLevel)::numChildren) {
                    merge(lNode, rNode, lSize, rSize, data.values.nums[numChildren - 2], data.values.ones[numChildren - 2], lastNums, lastOnes);
                    --idx;
                }
                break;
            }
            case numChildren - 2: {
                NODE_UN(lowerLevel) *lNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
                NODE_UN(lowerLevel) *mNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx)];
                NODE_UN(lowerLevel) *rNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)];
                const uint_fast8_t lSize = lNode->size(), mSize = mNode->size(), rSize = rNode->size();
                if(lSize < rSize) {
                    if(lSize + mSize <= NODE_UN(lowerLevel)::numChildren) {
                        merge(lNode, mNode, lSize, mSize, data.values.nums[idx - 1], data.values.ones[idx - 1], data.values.nums[idx], data.values.ones[idx]);
                        --idx;
                    }
                } else {
                    if(mSize + rSize <= NODE_UN(lowerLevel)::numChildren) {
                        merge(mNode, rNode, mSize, rSize, data.values.nums[idx], data.values.ones[idx], lastNums, lastOnes);
                    }
                }
                break;
            }
            default: {
                NODE_UN(lowerLevel) *lNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx - 1)];
                NODE_UN(lowerLevel) *mNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx)];
                NODE_UN(lowerLevel) *rNode = &RCNODE_UN(lowerLevel)(getChildrenPtr())[getIdx(meta, idx + 1)];
                const uint_fast8_t lSize = lNode->size(), mSize = mNode->size(), rSize = rNode->size();
                if(idx != _size - 1) {
                    if(lSize < rSize) {
                        if(lSize + mSize <= NODE_UN(lowerLevel)::numChildren) {
                            merge(lNode, mNode, lSize, mSize, data.values.nums[idx - 1], data.values.ones[idx - 1], data.values.nums[idx], data.values.ones[idx]);
                            --idx;
                        }
                    } else {
                        if(mSize + rSize <= NODE_UN(lowerLevel)::numChildren) {
                            merge(mNode, rNode, mSize, rSize, data.values.nums[idx], data.values.ones[idx], data.values.nums[idx + 1], data.values.ones[idx + 1]);
                        }
                    }
                } else {
                    if(lSize + mSize <= NODE_UN(lowerLevel)::numChildren) {
                        merge(lNode, mNode, lSize, mSize, data.values.nums[idx - 1], data.values.ones[idx - 1], data.values.nums[idx], data.values.ones[idx]);
                        --idx;
                    }
                }
                break;
            }
        }
        clearMetadata();
        denormalize(_size);
        writeMetaData<false>(meta);
        setSize(_size);
        return out;
    }

    template<class intType, bool isLowest>
    inline void moveLeft(NODE4 *lNode, const uint_fast8_t delta, const uint_fast8_t lSize, const uint_fast8_t rSize, intType &lNums, intType &rNums, intType &lOnes, intType &rOnes) {
        static_assert(is_integral_v<intType>);
        static_assert(!is_signed_v<intType>);
        using T = std::conditional_t<isLowest, NODE0, NODE2 >;
        const uint64_t lmeta = lNode->template readMetaData<false>(), rmeta = readMetaData<false>();
        ASSERT_LE(lSize + delta, numChildren)
        ASSERT_NE(lNode, this)
        lNode->clearMetadata(), clearMetadata();
        lNode->normalize(lSize), normalize(rSize);
        for(int i = 0; i < delta - 1; ++i) {
            const uint64_t curNums = data.values.nums[i], curOnes = data.values.ones[i];
            lNode->data.values.nums[lSize + i] = curNums;
            lNode->data.values.ones[lSize + i] = curOnes;
            lNums += curNums, lOnes += curOnes;
            rNums -= curNums, rOnes -= curOnes;
            std::memcpy(lNode->getChildrenPtr() + (getIdx(lmeta, lSize + i) * sizeof(T)), getChildrenPtr() + (getIdx(rmeta, i) * sizeof(T)), sizeof(T));
        }
        if(lSize + delta == numChildren) {
            const uint64_t curNums = data.values.nums[delta - 1], curOnes = data.values.ones[delta - 1];
            lNums += curNums, lOnes += curOnes;
            rNums -= curNums, rOnes -= curOnes;
            std::memcpy(lNode->getChildrenPtr() + (getIdx(lmeta, lSize + delta - 1) * sizeof(T)), getChildrenPtr() + (getIdx(rmeta, delta - 1) * sizeof(T)), sizeof(T));
        } else {
            const uint64_t curNums = data.values.nums[delta - 1], curOnes = data.values.ones[delta - 1];
            lNode->data.values.nums[lSize + delta - 1] = curNums;
            lNode->data.values.ones[lSize + delta - 1] = curOnes;
            lNums += curNums, lOnes += curOnes;
            rNums -= curNums, rOnes -= curOnes;
            std::memcpy(lNode->getChildrenPtr() + (getIdx(lmeta, lSize + delta - 1) * sizeof(T)), getChildrenPtr() + (getIdx(rmeta, delta - 1) * sizeof(T)), sizeof(T));

        }
        intType copyFullNums = rNums, copyFullOnes = rOnes;
        for(int i = 0; i < rSize - delta - 1; ++i) {
            const uint64_t curNums = data.values.nums[i + delta], curOnes = data.values.ones[i + delta];
            data.values.nums[i] = curNums;
            data.values.ones[i] = curOnes;
            data.values.nums[i + delta] = 0xffff'ffff'ffff'ffff;
            data.values.ones[i + delta] = 0xffff'ffff'ffff'ffff;
            copyFullNums -= curNums;
            copyFullOnes -= curOnes;
        }
        if(rSize != numChildren) {
            data.values.nums[rSize - delta - 1] = data.values.nums[rSize - 1];
            data.values.ones[rSize - delta - 1] = data.values.ones[rSize - 1];
            for(int i = rSize - delta; i < numChildren - 1; ++i) {
                data.values.nums[i] = 0xffff'ffff;
                data.values.ones[i] = 0xffff'ffff;
            }
        } else {
            data.values.nums[rSize - delta - 1] = copyFullNums;
            data.values.ones[rSize - delta - 1] = copyFullOnes;
        }
        ASSERT_CODE_LOCALE(if constexpr(isLowest) {
            for(int i = 0; i < numChildren - 1; ++i) {
                ASSERT_PR(data.values.nums[i] != 0xffff'ffff, data.values.nums[i], wordSize * wordSize / 2, wordSize * wordSize + 1)
            }
        })
        lNode->clearMetadata(), clearMetadata();
        lNode->denormalize(lSize + delta), denormalize(rSize - delta);
        lNode->writeMetaData<false>(lmeta), writeMetaData<false>(rotateLeft(rmeta, delta));
        lNode->setSize(lSize + delta), setSize(rSize - delta);
    }

    template<class intType, bool isLowest>
    inline void moveRight(NODE4 *rNode, const uint_fast8_t delta, const uint_fast8_t lSize, const uint_fast8_t rSize, intType &lNums, intType &rNums, intType &lOnes, intType &rOnes) {
        static_assert(is_integral_v<intType>);
        static_assert(!is_signed_v<intType>);
        using T = std::conditional_t<isLowest, NODE0, NODE2 >;
        //  ASSERT_B(!isMetaCleared<true>() && !isMetaCleared<false>() && !rNode->isMetaCleared<true>() && !rNode->isMetaCleared<false>())
        const uint64_t lmeta = readMetaData<false>(), rmeta = rotateRight(rNode->template readMetaData<false>(), delta);
        ASSERT_LE(rSize + delta, numChildren)
        ASSERT_GE(lSize - delta, numChildren / 2)
        ASSERT_NE(rNode, this)
        rNode->clearMetadata(), clearMetadata();
        rNode->normalize(rSize), normalize(lSize);
        for(int i = numChildren - 2; i >= delta; --i) {
            rNode->data.values.nums[i] = rNode->data.values.nums[i - delta];
            rNode->data.values.ones[i] = rNode->data.values.ones[i - delta];
        }
        for(int i = rSize + delta; i < numChildren - 1; ++i) {
            rNode->data.values.nums[i] = 0xffff'ffff;
            rNode->data.values.ones[i] = 0xffff'ffff;
        }
        for(int i = 0; i < delta - (lSize == numChildren); ++i) {
            const uint64_t curNums = data.values.nums[i + lSize - delta], curOnes = data.values.ones[i + lSize - delta];
            data.values.nums[i + lSize - delta] = 0xffff'ffff'ffff'ffff;
            data.values.ones[i + lSize - delta] = 0xffff'ffff'ffff'ffff;
            rNode->data.values.nums[i] = curNums;
            rNode->data.values.ones[i] = curOnes;
            lNums -= curNums, lOnes -= curOnes;
            rNums += curNums, rOnes += curOnes;
            std::memcpy(rNode->getChildrenPtr() + (getIdx(rmeta, i) * sizeof(T)), getChildrenPtr() + (getIdx(lmeta, i + lSize - delta) * sizeof(T)), sizeof(T));
        }
        if(lSize == numChildren) {
            intType copyFullNums = lNums, copyFullOnes = lOnes;
            for(int i = 0; i < lSize - delta; ++i) {
                copyFullNums -= data.values.nums[i];
                copyFullOnes -= data.values.ones[i];
            }
            rNode->data.values.nums[delta - 1] = copyFullNums;
            rNode->data.values.ones[delta - 1] = copyFullOnes;
            lNums -= copyFullNums;
            lOnes -= copyFullOnes;
            rNums += copyFullNums;
            rOnes += copyFullOnes;
        }
        std::memcpy(rNode->getChildrenPtr() + (getIdx(rmeta, delta - 1) * sizeof(T)), getChildrenPtr() + (getIdx(lmeta, lSize - 1) * sizeof(T)), sizeof(T));
        ASSERT_CODE_LOCALE(if constexpr(isLowest) {
            for(int i = 0; i < numChildren - 1; ++i) {
                ASSERT_PR(data.values.nums[i] != 0xffff, data.values.nums[i], wordSize * wordSize / 2, wordSize * wordSize + 1)
            }
            for(int i = 0; i < rSize + delta && i < numChildren - 1; ++i) {
                ASSERT_R(RCNODE_0(rNode->getChildrenPtr())[getIdx(rmeta, i)].template rank<true>(rNode->data.values.nums[i] - (rNode->data.values.nums[i] == wordSize * wordSize)), rNode->data.values.ones[i] - 1, rNode->data.values.ones[i] + 1, i)
            }
            for(int i = 0; i < lSize - delta && i < numChildren - 1; ++i) {
                ASSERT_R(RCNODE_0(getChildrenPtr())[getIdx(lmeta, i)].template rank<true>(data.values.nums[i] - (data.values.nums[i] == wordSize * wordSize)), data.values.ones[i] - 1, data.values.ones[i] + 1, i)
            }
        } else {
            for(int i = 0; i < delta; ++i) {
                uint64_t *rval = rc<uint64_t *>(rNode->getChildrenPtr()) + (8 * getIdx(rmeta, i));
                uint64_t *lval = rc<uint64_t *>(getChildrenPtr()) + (8 * getIdx(lmeta, i + lSize - delta));
                for(int j = 0; j < 8; ++j) {
                    ASSERT_E(rval[j], lval[j], i, j, sizeof(T))
                }
            }
        })
        clearMetadata(), rNode->clearMetadata();
        denormalize(lSize - delta), rNode->denormalize(rSize + delta);
        writeMetaData<false>(lmeta), rNode->writeMetaData<false>(rmeta);
        setSize(lSize - delta), rNode->setSize(rSize + delta);
    }


    template<class intType, bool isLowest>
    inline void moveInit(NODE4 *lnode, uint_fast8_t delta, uint_fast8_t lSize, uint8_t *&curChild, uint_fast8_t &upperindex, uint_fast8_t &index, intType &lNextNums, intType &rNextNums, intType &lNextOnes, intType &rNextOnes) {
        static_assert(is_integral_v<intType>);
        static_assert(!is_signed_v<intType>);
        ASSERT_R(scu32(lnode->size()), numChildren - 1, numChildren + 1)
        ASSERT_E(rNextNums, 0)
        ASSERT_E(rNextOnes, 0)
        using T = std::conditional_t<isLowest, NODE3, NODE4 >;
        T *leaves;
        if constexpr(isLowest)leaves = DYNAMIC_BV::template allocateNode<4, 3>(); else leaves = DYNAMIC_BV::template allocateNode<4, 4>();
        const uint64_t thisMeta = 0x3210, lMeta = lnode->readMetaData<false>();
        lnode->clearMetadata();
        lnode->normalize(lSize);
        for(int i = delta; i < numChildren - 1; ++i) {
            data.values.nums[i] = 0xffff'ffff'ffff'ffff;
            data.values.ones[i] = 0xffff'ffff'ffff'ffff;
        }
        for(int i = 0; i < delta - 1; ++i) {
            const uint64_t curNums = lnode->data.values.nums[i + lSize - delta], curOnes = lnode->data.values.ones[i + lSize - delta];
            lnode->data.values.nums[i + lSize - delta] = 0xffff'ffff'ffff'ffff;
            lnode->data.values.ones[i + lSize - delta] = 0xffff'ffff'ffff'ffff;
            data.values.nums[i] = curNums;
            data.values.ones[i] = curOnes;
            lNextNums -= curNums;
            lNextOnes -= curOnes;
            rNextNums += curNums;
            rNextOnes += curOnes;
            std::memcpy(leaves + (getIdx(thisMeta, i)), lnode->getChildrenPtr() + (getIdx(lMeta, i + lSize - delta) * sizeof(T)), sizeof(T));
        }
        intType copyFullNums = lNextNums, copyFullOnes = lNextOnes;
        for(int i = 0; i < lSize - delta; ++i) {
            copyFullNums -= lnode->data.values.nums[i];
            copyFullOnes -= lnode->data.values.ones[i];
        }
        data.values.nums[delta - 1] = copyFullNums;
        data.values.ones[delta - 1] = copyFullOnes;
        lNextNums -= copyFullNums;
        lNextOnes -= copyFullOnes;
        rNextNums += copyFullNums;
        rNextOnes += copyFullOnes;
        if(index >= lSize - delta) {
            ++upperindex;
            index -= lSize - delta;
            curChild = rcbyte(this);
        }
        std::memcpy(leaves + (getIdx(thisMeta, delta - 1)), lnode->getChildrenPtr() + (getIdx(lMeta, lSize - 1) * sizeof(T)), sizeof(T));
        lnode->clearMetadata();
        lnode->denormalize(lSize - delta);
        lnode->setSize(lSize - delta);
        lnode->template writeMetaData<false>(lMeta);
        clearMetadata();
        denormalize(delta);
        setSize(delta);
        writeMetaData<false>(thisMeta);
        setChildrenPtr(rcbyte(leaves));
    }

    template<uint8_t length>
    inline void init(const uint64_t (&nums)[length], const uint64_t (&ones)[length]) {
        static_assert(length <= numChildren);
        int i = 0;
        for(; i < length; ++i) {
            ASSERT_GE(__builtin_clzll(nums[i]), 3)
            ASSERT_GE(__builtin_clzll(ones[i]), 3)
            data.values.nums[i] = nums[i];
            data.values.ones[i] = ones[i];
        }

        for(; i < NODE4::numChildren - 1; ++i) {
            data.values.nums[i] = 0xffff'ffff'ffff'ffffull;
            data.values.ones[i] = 0xffff'ffff'ffff'ffffull;
        }
        clearMetadata();
        denormalize(length);
        setSize(length);
        writeMetaData<false>(0x3210);
    }


    template<typename ArrType, typename SizeType>
    inline uint64_t init(NO_ASSERT_CODE(const) SameValueArray<ArrType, SizeType> &numsVec, const uint64_t *onesVec, int begin, int length) {
        ASSERT_E(data.values.rest, 0)
        ASSERT_LE(length, numChildren)
        ASSERT_CODE_GLOBALE(uint64_t numCount = 0, oneCount = 0;)
        int i = 0;
        uint64_t out = 0;
        for(; i < length; ++i, ++begin) {
            out += onesVec[begin];
            data.values.nums[i] = numsVec[begin];
            data.values.ones[i] = onesVec[begin];
        }
        for(; i < NODE4::numChildren - 1; ++i) {
            data.values.nums[i] = 0xffff'ffff'ffff'ffff;
            data.values.ones[i] = 0xffff'ffff'ffff'ffff;
        }
        clearMetadata();
        denormalize(length);
        setSize(length);
        writeMetaData<false>(0x3210);
        return out;
    }

    //1,2,3
    //0,1,0
    inline uint_fast8_t getIdx(uint64_t &key) const {
        // uint64_t meta = readMetaData<false>();
        if constexpr(useBinarySearch) {
            //{0,1,2,3}
            if(key < data.values.nums[1]) {
                //{0,1}
                if(key < data.values.nums[0]) {
                    return 0;
                } else {
                    key -= data.values.nums[0];
                    return 1;
                }
            } else {
                //{2,3}
                if((key -= data.values.nums[1]) < data.values.nums[2]) {
                    return 2;
                } else {
                    key -= data.values.nums[2];
                    return 3;
                }
            }
        } else {
            uint_fast8_t i = 0;
            uint64_t prevkey = key;
            for(; (key -= data.values.nums[i]) < prevkey; ++i, prevkey = key);
            key = prevkey;
            return i;
        }
    }

    inline uint_fast8_t getIdx(uint64_t &key, uint64_t &curNums, uint64_t &curOnes) const {
        // uint64_t meta = readMetaData<false>();
        if constexpr(useBinarySearch) {
            //{0,1,2,3}
            if(key <= data.values.nums[1]) {
                //{0,1}
                if(key <= data.values.nums[0]) {
                    curNums = data.values.nums[0];
                    curOnes = data.values.ones[0];
                    return 0;
                } else {
                    curNums = data.values.nums[1] - data.values.nums[0];
                    curOnes = data.values.ones[1] - data.values.ones[0];
                    key -= data.values.nums[0];
                    return 1;
                }
            } else {
                //{2,3}
                if((key -= data.values.nums[1]) <= data.values.nums[2]) {
                    curNums = data.values.nums[2];
                    curOnes = data.values.ones[2];
                    return 2;
                } else {
                    curNums = data.values.nums[1] - data.values.nums[2];
                    curOnes = data.values.ones[1] - data.values.ones[2];
                    key -= data.values.nums[2];
                    return 3;
                }
            }
        } else {
            uint_fast8_t i = 0;
            uint64_t prevkey = key;
            for(; (key -= data.values.nums[i]) < prevkey; ++i, prevkey = key)curNums -= data.values.nums[i];
            key = prevkey;
            return i;
        }
    }


    template<bool rank, bool one>
    inline pair<uint_fast8_t, uint64_t> select_rankIdx(uint64_t &key) {
        // uint64_t meta = readMetaData<false>(),
        uint64_t cnums = 0;
        auto arrayFunction = [this](uint_fast8_t idx) constexpr {
            if constexpr(rank) {
                return data.values.nums[idx];
            } else {
                if constexpr(one) {
                    return data.values.ones[idx];
                } else {
                    return data.values.nums[idx] - data.values.ones[idx];
                }
            }
        };
        auto alterFunction = [this](uint_fast8_t idx) constexpr {
            if constexpr(!rank) {
                return data.values.nums[idx];
            } else {
                if constexpr(one) {
                    return data.values.ones[idx];
                } else {
                    return data.values.nums[idx] - data.values.ones[idx];
                }
            }
        };
        uint_fast8_t _size = size();
        if constexpr(useBinarySearch) {
            //{0,1,2,3}
            if((_size <= 2) | (key < arrayFunction(1))) {
                //{0,1}
                if((_size <= 1) | (key < arrayFunction(0))) {
                    return {0, cnums};
                } else {
                    key -= arrayFunction(0);
                    cnums += alterFunction(0);
                    return {1, cnums};
                }
            } else {
                //{2,3}
                cnums += alterFunction(1);
                if((key -= arrayFunction(1)) < arrayFunction(2)) {
                    return {2, cnums};
                } else {
                    cnums += alterFunction(2);
                    key -= arrayFunction(2);
                    return {3, cnums};
                }
            }
        } else {
            uint_fast8_t i = 0;
            uint64_t prevkey = key;
            for(; (key -= arrayFunction(i)) < prevkey; prevkey = key, ++i)
                cnums += alterFunction(i);
            key = prevkey;
            return {i, cnums};
        }
    }

    template<bool inc>
    inline void incDecNums(uint_fast8_t idx) {
        ASSERT_CODE_GLOBALE(uint64_t assert_meta = readMetaData<false>();)
        const uint_fast8_t _size = size();
        if constexpr(useBinarySearch) {
            for(; idx < _size - (_size == numChildren); idx += (1 << ctzLookup[idx])) {
                if constexpr(inc) { ++data.values.nums[idx]; } else { --data.values.nums[idx]; }
            }
        } else {
            if constexpr(inc) { ++data.values.nums[idx]; } else { --data.values.nums[idx]; }
        }
        ASSERT_E(assert_meta, readMetaData<false>())
    }

    template<bool inc>
    inline void incDecOnes(uint_fast8_t idx) {
        ASSERT_CODE_GLOBALE(uint64_t assert_meta = readMetaData<false>();)
        if constexpr(useBinarySearch) {
            const uint_fast8_t _size = size();
            for(; idx < _size - (_size == numChildren); idx += (1 << ctzLookup[idx])) {
                if constexpr(inc) {
                    ++data.values.ones[idx];
                } else {
                    --data.values.ones[idx];
                }
            }
        } else {
            if constexpr(inc) {
                ++data.values.ones[idx];
            } else {
                --data.values.ones[idx];
            }
        }
        ASSERT_E(assert_meta, readMetaData<false>())
    }

    static void test() {
        {
            const uint64_t meta = 0x3210;
            ASSERT_E(NODE4::pRemove(meta, 0), 0x0321)
            ASSERT_E(NODE4::pRemove(meta, 1), 0x1320)
            ASSERT_E(NODE4::pRemove(meta, 2), 0x2310)
            ASSERT_E(NODE4::pRemove(meta, 3), 0x3210)
        }
        {
            const uint64_t meta = 0x3210;
            ASSERT_E(NODE4::pInsert(meta, 0), 0x2103)
            ASSERT_E(NODE4::pInsert(meta, 1), 0x2130)
            ASSERT_E(NODE4::pInsert(meta, 2), 0x2310)
            ASSERT_E(NODE4::pInsert(meta, 3), 0x3210)
        }
        //
        {
            const uint64_t meta = 0x3210;
            ASSERT_E(NODE4::rotateLeft(meta, 0), 0x3210)
            ASSERT_E(NODE4::rotateLeft(meta, 1), 0x0321)
            ASSERT_E(NODE4::rotateLeft(meta, 2), 0x1032)
            ASSERT_E(NODE4::rotateLeft(meta, 3), 0x2103)
        }
        //
        {
            const uint64_t meta = 0x3210;
            ASSERT_E(NODE4::rotateRight(meta, 0), 0x3210)
            ASSERT_E(NODE4::rotateRight(meta, 1), 0x2103)
            ASSERT_E(NODE4::rotateRight(meta, 2), 0x1032)
            ASSERT_E(NODE4::rotateRight(meta, 3), 0x0321)
        }
    }

};

#endif //ADS_DYNAMICBITVECTOR_H
