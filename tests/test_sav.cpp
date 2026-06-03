// test_sav.cpp — host test for savestate layer
// Build: g++ -std=c++17 -Iinclude src/sav.cpp tests/test_sav.cpp -o test_sav
// Run: ./test_sav

#include "sav.h"

#include <cstdio>

int main()
{
    return sav_test();
}
