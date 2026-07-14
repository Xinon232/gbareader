// test_vocab_10k.cpp — verify the streaming index accepts exactly 10,000 cards
// Build: ./tests/build_host.sh src/vocab.cpp tests/test_vocab_10k.cpp -o /tmp/test_vocab_10k
// Run: /tmp/test_vocab_10k

#include "vocab.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main()
{
    constexpr int expected_capacity = 10000;
    constexpr int input_lines = expected_capacity + 1;
    const int buffer_size = input_lines * 40 + 1024;
    char* data = static_cast<char*>(std::malloc(buffer_size));
    if (! data) {
        std::fprintf(stderr, "FAIL: malloc\n");
        return 1;
    }

    int used = 0;
    for (int index = 0; index < input_lines; ++index) {
        int written = std::snprintf(data + used, buffer_size - used,
                                    "word_%05d\ttarget_%05d\n", index, index);
        if (written <= 0 || written >= buffer_size - used) {
            std::fprintf(stderr, "FAIL: source overflow at %d\n", index);
            std::free(data);
            return 1;
        }
        used += written;
    }

    VocabFile vocab;
    int loaded = vocab_open(vocab, data, used);
    if (VOCAB_MAX_LINES != expected_capacity || loaded != expected_capacity ||
        vocab.line_count != expected_capacity ||
        vocab.field_counts[0] != expected_capacity) {
        std::fprintf(stderr,
                     "FAIL: capacity=%d loaded=%d count=%d field1=%u expected=%d\n",
                     VOCAB_MAX_LINES, loaded, vocab.line_count,
                     vocab.field_counts[0], expected_capacity);
        std::free(data);
        return 1;
    }

    LineBuf last;
    if (! vocab_show(vocab, data, used, expected_capacity - 1, last) ||
        std::strcmp(last.a, "word_09999") != 0 ||
        std::strcmp(last.b, "target_09999") != 0) {
        std::fprintf(stderr, "FAIL: final supported card was not indexed correctly\n");
        std::free(data);
        return 1;
    }

    if (vocab_show(vocab, data, used, expected_capacity, last)) {
        std::fprintf(stderr, "FAIL: card beyond the 10,000-entry cap was indexed\n");
        std::free(data);
        return 1;
    }

    std::printf("PASS: exactly %d entries indexed; entry 10,001 rejected\n",
                expected_capacity);
    std::free(data);
    return 0;
}
