#ifndef ADS_TESTUTILS_H
#define ADS_TESTUTILS_H

#include<bits/stdc++.h>

#define endl '\n'
using namespace std;

void randomInit(int n) {
    cout << n << endl;
    for(int i = 0; i < n; ++i) {
        cout << rand() % 2 << endl;
    }

}

void winsert(uint32_t idx, bool b) {
    cout << "pInsert " << idx << " " << static_cast<int>(b) << endl;
}

void wdelete(uint32_t idx) { cout << "delete " << idx << endl; }

void flip(uint32_t idx) { cout << "flip " << idx << endl; }

void rank(uint32_t idx, bool b) { cout << "rank " << static_cast<int>(b) << " " << idx << endl; }

void select(uint32_t idx, bool b) { cout << "select " << static_cast<int>(b) << " " << idx << endl; }

void getByte(uint32_t idx) { cout << "get " << idx << endl; }

#endif //ADS_TESTUTILS_H
