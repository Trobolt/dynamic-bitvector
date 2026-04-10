#ifndef ADS_DYNBITVECTEST_CPP
#define ADS_DYNBITVECTEST_CPP

#include<bits/stdc++.h>
#include "dynamicbitvector.h"
#include "../testutils.h"
#include "../myassert.h"

using namespace std;


int main() {
    uint32_t curmax = 10;
    slowDynBitVec dbv(0);
    cout << 0 << endl;
    for (int i = 0; i < curmax; ++i) {
        uint32_t idx = rand() % (i + 1);
        bool b = rand() & 1;
        winsert(idx, b);
        if (b) {
            dbv.insert<1>(idx);
            assert(dbv.getBit(idx) == true);
        } else {
            dbv.insert<0>(idx);
            assert(dbv.getBit(idx) == false);
        }
    }
    for (int i = curmax - 2; i > 0; --i) {
        int idx = rand() % (i);
        wdelete(idx);
        bool b = dbv.getBit(idx + 1);
        dbv.deleteB(idx);
        assert(dbv.getBit(idx) == b);
    }
    while (dbv.size() > 0)
        dbv.deleteB(0);

}


#endif //ADS_DYNBITVECTEST_CPP
