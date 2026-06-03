// test_vocab_5k.cpp — synthesize a 5000-word file and run the streaming test
// Build: g++ -std=c++17 -Iinclude src/vocab.cpp tests/test_vocab_5k.cpp -o test_5k
// Run: ./test_5k
//
// This is the design-target scale. 5000 flashcards = ~150KB on disk.
// The streaming model must handle it without blowing RAM.

#include "vocab.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

int main()
{
    // Synthesize a 5000-word file. Use simple word stems.
    const int N = 5000;
    const int buf_size = N * 40 + 1024;  // ~30 bytes/word + slack
    char* data = (char*)malloc(buf_size);
    if (!data) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    srand(42);
    int off = 0;
    for (int i = 0; i < N; i++) {
        // Each line: "word_NNN\ttarget_NNN\n"
        int n = snprintf(data + off, buf_size - off, "word_%05d\ttarget_%05d\n", i, i);
        if (n <= 0 || n >= buf_size - off) {
            fprintf(stderr, "snprintf overflow at i=%d\n", i);
            free(data);
            return 1;
        }
        off += n;
    }
    printf("Synthesized %d lines, %d bytes\n", N, off);

    int rc = vocab_streaming_test(data, off);
    free(data);
    return rc;
}
