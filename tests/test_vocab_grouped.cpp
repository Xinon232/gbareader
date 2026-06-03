// test_vocab_grouped.cpp — grouped dict.cc import/export host tests
#include "vocab.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

static int expect_counts(const VocabFile& vf, int a, int b, int c, int d, int e)
{
    int expected[5] = {a, b, c, d, e};
    for (int i = 0; i < 5; i++) {
        if (vf.field_counts[i] != expected[i]) {
            printf("    FAIL: field_counts[%d] got %u expected %d\n", i, vf.field_counts[i], expected[i]);
            return 1;
        }
    }
    return 0;
}

static int test_grouped_import_blank_lines_map_to_fields()
{
    printf("[1] grouped import maps blank groups to fields\n");
    const char* data =
        "a1\tb1\r\n"
        "a2\tb2\r\n"
        "\r\n"
        "\r\n"
        "a3\tb3\r\n"
        "\r\n"
        "a4\tb4\r\n"
        "a5\tb5\r\n"
        "\r\n";
    VocabFile vf;
    int loaded = vocab_open(vf, data, (int)strlen(data));
    if (loaded != 5) { printf("    FAIL: loaded %d expected 5\n", loaded); return 1; }
    if (expect_counts(vf, 2, 1, 2, 0, 0)) return 1;
    if (vf.field[0] != 1 || vf.field[1] != 1 || vf.field[2] != 2 || vf.field[3] != 3 || vf.field[4] != 3) {
        printf("    FAIL: fields = %u,%u,%u,%u,%u\n", vf.field[0], vf.field[1], vf.field[2], vf.field[3], vf.field[4]);
        return 1;
    }
    printf("    OK\n");
    return 0;
}

static int test_long_realistic_line_parses()
{
    printf("[2] long realistic dict.cc line parses\n");
    char a[80]; char b[90];
    memset(a, 'a', 67); a[67] = 0;
    memset(b, 'b', 77); b[77] = 0;
    char line[192];
    int len = snprintf(line, sizeof(line), "%s\t%s\r\n", a, b);
    VocabFile vf;
    int loaded = vocab_open(vf, line, len);
    if (loaded != 1) { printf("    FAIL: loaded %d expected 1\n", loaded); return 1; }
    LineBuf lb;
    if (!vocab_show(vf, line, len, 0, lb)) { printf("    FAIL: vocab_show failed\n"); return 1; }
    if ((int)strlen(lb.a) != 67 || (int)strlen(lb.b) != 77) {
        printf("    FAIL: lengths %zu/%zu expected 67/77\n", strlen(lb.a), strlen(lb.b));
        return 1;
    }
    printf("    OK\n");
    return 0;
}

static int test_grouped_export_reopen_preserves_fields()
{
    printf("[3] grouped export/reopen preserves field counts\n");
    const char* data =
        "one\teins\r\n"
        "two\tzwei\r\n"
        "three\tdrei\r\n";
    VocabFile vf;
    int loaded = vocab_open(vf, data, (int)strlen(data));
    if (loaded != 3) return 1;
    vocab_advance(vf, 0);          // one -> field 2
    vocab_advance(vf, 1);          // two -> field 2
    vocab_advance(vf, 1);          // two -> field 3
    char out[512];
    int written = vocab_export_grouped(vf, data, (int)strlen(data), out, sizeof(out));
    if (written <= 0) { printf("    FAIL: export returned %d\n", written); return 1; }
    VocabFile vf2;
    int reloaded = vocab_open(vf2, out, written);
    if (reloaded != 3) { printf("    FAIL: reloaded %d expected 3\n", reloaded); return 1; }
    if (expect_counts(vf2, 1, 1, 1, 0, 0)) return 1;
    LineBuf lb;
    if (!vocab_show(vf2, out, written, 0, lb) || strcmp(lb.a, "three") != 0) {
        printf("    FAIL: field 1 first card should be three, got %s\n", lb.a);
        return 1;
    }
    printf("    OK\n");
    return 0;
}

static int test_latin1_umlaut_is_preserved_for_gba_font()
{
    printf("[4] Latin-1 umlaut display is preserved\n");
    const char data[] = "ardilla {f}\tEichh\xF6rnchen {n}\r\n";
    VocabFile vf;
    int loaded = vocab_open(vf, data, (int)strlen(data));
    if (loaded != 1) { printf("    FAIL: loaded %d expected 1\n", loaded); return 1; }
    LineBuf lb;
    if (!vocab_show(vf, data, (int)strlen(data), 0, lb)) { printf("    FAIL: vocab_show failed\n"); return 1; }
    if (strcmp(lb.a, "ardilla {f}") != 0 || strcmp(lb.b, "Eichhörnchen {n}") != 0) {
        printf("    FAIL: got '%s' / '%s'\n", lb.a, lb.b);
        return 1;
    }
    printf("    OK\n");
    return 0;
}

static int test_utf8_russian_and_arabic_are_preserved()
{
    printf("[5] UTF-8 Russian and Arabic display is preserved\n");
    const char data[] = "casa\tдом\r\nagua\tماء\r\nwrite\tكتب\r\n";
    VocabFile vf;
    int loaded = vocab_open(vf, data, (int)strlen(data));
    if (loaded != 3) { printf("    FAIL: loaded %d expected 3\n", loaded); return 1; }
    LineBuf lb;
    if (!vocab_show(vf, data, (int)strlen(data), 0, lb) || strcmp(lb.b, "дом") != 0) {
        printf("    FAIL: Russian got '%s' / '%s'\n", lb.a, lb.b);
        return 1;
    }
    if (!vocab_show(vf, data, (int)strlen(data), 1, lb) || strcmp(lb.b, "ﺀﺎﻣ") != 0) {
        printf("    FAIL: Arabic got '%s' / '%s'\n", lb.a, lb.b);
        return 1;
    }
    if (!vocab_show(vf, data, (int)strlen(data), 2, lb) || strcmp(lb.b, "ﺐﺘﻛ") != 0) {
        printf("    FAIL: Arabic joining got '%s' / '%s'\n", lb.a, lb.b);
        return 1;
    }
    printf("    OK\n");
    return 0;
}

int main()
{
    int rc = 0;
    rc |= test_grouped_import_blank_lines_map_to_fields();
    rc |= test_long_realistic_line_parses();
    rc |= test_grouped_export_reopen_preserves_fields();
    rc |= test_latin1_umlaut_is_preserved_for_gba_font();
    rc |= test_utf8_russian_and_arabic_are_preserved();
    if (rc) { printf("\nFAIL\n"); return 1; }
    printf("\nPASS: grouped vocab import/export\n");
    return 0;
}
