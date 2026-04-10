#include <iostream>

#include<bits/stdc++.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <iostream>
#include <filesystem>
#include <iostream>
#include <fstream>

#include "bitutils.h"
#include "DynamicBitVector/dynamicbitvector.h"

template<WordSizes w = WORDSIZE32BIT, bool test = false>
static uintmax_t bitvecmain(const char *inFile, const char *outFile) {
    std::ifstream inStream(inFile);
    std::conditional_t<test, std::ifstream, std::ofstream> outStream(outFile);
    int n;
    inStream >> n;
    BitVectorBuilder<w, true> bv(n);

    bool b;
    for(int i = 0; i < n; ++i) {
        inStream >> b;
        bv.push_back(b);
    }

    DynamicBitVector<w, true> dbv(bv);

    std::string line, str;
    uint64_t key;
    std::getline(inStream, line);
    for(int i = 0; std::getline(inStream, line); ++i) {
        std::istringstream iss(line);
        iss >> str;
        if(str == "insert") {
            iss >> key;
            iss >> b;
            if(b) {
                dbv.template insert<true>(key);
            } else {
                dbv.template insert<false>(key);
            }
        } else if(str == "delete") {
            iss >> key;
            dbv.remove(key);
        } else if(str == "flip") {
            iss >> key;
            dbv.flip(key);
        } else if(str == "rank") {
            iss >> b;
            iss >> key;
            uint64_t idx;
            if(b) {
                idx = dbv.template rank<true>(key);
            } else {
                idx = dbv.template rank<false>(key);
            }
            if constexpr(test) {
                uint64_t temp;
                outStream >> temp;
                ASSERT_E(temp, idx)
            } else {
                outStream << idx << '\n';
            }
        } else if(str == "select") {
            iss >> b;
            iss >> key;
            uint64_t idx;
            if(b) {
                idx = dbv.template select<true>(key);
            } else {
                idx = dbv.template select<false>(key);
            }
            if constexpr(test) {
                uint64_t temp;
                outStream >> temp;
                ASSERT_E(idx, temp)
            } else {
                outStream << idx << '\n';
            }
        } else {
            string str2;
            (iss >> str2);
            ASSERT_UNREACHEABLE("falscher Befehl.", str, (str2))
        }
        // process pair (a,b)
    }
    return dbv.sizeInBytes() * 8;
}

int main(int argc, char **argv) {
    ALWAYS_ASSERT_E(argc, 5)
    ALWAYS_ASSERT_B(!strcmp(argv[1], "ads_programm_a\0"))
    if(!strcmp(argv[2], "bv\0")) {
        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        uint64_t size = bitvecmain<WORDSIZE32BIT, false>(argv[3], argv[4]);
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        cout << "RESULT algo=bv name<tobias_paweletz> time=<" << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "> space=<" << size << ">" << endl;
    } else if(!strcmp(argv[1], "bp")) {
        ASSERT_UNIMPLEMETED()
    } else { ASSERT_UNREACHEABLE("Eingabe Fehler") }
    // std::ofstream file(cwd.string());
    //  file.close();
}
