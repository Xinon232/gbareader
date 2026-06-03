// test_vocab.cpp — host-side test harness for the streaming vocab layer
// Build with: g++ -std=c++17 -Iinclude src/vocab.cpp tests/test_vocab.cpp -o test_vocab
// Run: ./test_vocab data/sample.txt

#include "vocab.h"

#include <cstdio>
#include <fstream>
#include <vector>

int main(int argc, char* argv[])
{
    const char* path = (argc > 1) ? argv[1] : "data/sample.txt";
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", path);
        return 1;
    }

    std::vector<char> data((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    f.close();

    printf("=== Vocab streaming test ===\n");
    printf("File: %s\n", path);
    printf("Size: %zu bytes\n", data.size());

    int rc = vocab_streaming_test(data.data(), (int)data.size());
    return rc;
}
