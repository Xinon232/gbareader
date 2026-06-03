
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "fatfs/ff.h"
#include "cheats.h"

// Fake FatFS implementation
FRESULT f_open (FIL* fp, const TCHAR* path, BYTE mode) {
  FILE *fd = fopen(path, "rb");
  if (!fd)
    return FR_NO_FILE;

  *(FILE**)fp = fd;
  return FR_OK;
}

FRESULT f_read (FIL* fp, void* buff, UINT btr, UINT* br) {
  FILE *fd = *(FILE**)fp;
  size_t ret = fread(buff, 1, btr, fd);
  if (ret < 0)
    return FR_DISK_ERR;
  *br = ret;
  return FR_OK;
}

FRESULT f_close (FIL* fp) {
  FILE *fd = *(FILE**)fp;
  fclose(fd);
  return FR_OK;
}


const t_cheat_predec cheat1[] = {{ .opcode = 3, .blen = 8, .value = 0x0123, .address = 0x0300ABCD}};
const t_cheat_predec cheat2[] = {{ .opcode = 8, .blen = 8, .value = 0xABCD, .address = 0x03123456}};
const t_cheat_predec cheat3[] = {
  { .opcode = 3, .blen = 8, .value = 0x0010, .address = 0x0300AB01},
  { .opcode = 3, .blen = 8, .value = 0x0020, .address = 0x0300AB02},
  { .opcode = 3, .blen = 8, .value = 0x0030, .address = 0x0300AB03},
  { .opcode = 3, .blen = 8, .value = 0x0040, .address = 0x0300AB04},
};
const t_cheat_predec cheat4[] = {{ .opcode = 4, .blen = 16, .value = 0x0101, .address = 0x03002C2E}};

const struct {
  const char *title;
  unsigned num_codes;
  const t_cheat_predec *codes;
} expected [] = {
  { "First cheat title", 1, cheat1 },
  { "Second cheat", 1, cheat2 },
  { "Third cheat", 4, cheat3 },
  { "Some real char using a slide code", 2, cheat4 },
};

int main() {
  uint8_t tmp[32*1024];

  // Check for corner cases
  assert(open_read_cheats(tmp, sizeof(tmp), "data/bad1.cht") < 0);
  assert(open_read_cheats(tmp, sizeof(tmp), "data/bad2.cht") < 0);
  assert(open_read_cheats(tmp, sizeof(tmp), "data/bad3.cht") < 0);
  assert(open_read_cheats(tmp, sizeof(tmp), "data/bad4.cht") < 0);

  int ret = open_read_cheats(tmp, sizeof(tmp), "data/test.cht");
  assert(ret >= 0);
  assert(*(uint32_t*)tmp == sizeof(expected)/sizeof(expected[0]));

  // Parse the output buffer, see that it matches what we expect
  int i = 4, n = 0;
  while (i < ret) {
    t_cheathdr *e = (t_cheathdr*)&tmp[i];
    assert(e->enabled == 0);
    assert(!strcmp(expected[n].title, (char*)e->data));
    assert(e->slen == ((strlen(expected[n].title) + 1 + 3) & ~3U));
    assert(e->codelen == (expected[n].num_codes + 1) * 8);

    // Validate the opcodes
    unsigned off = 0;
    for (unsigned j = 0; j < expected[n].num_codes; j++) {
      t_cheat_predec * pc = (t_cheat_predec*)&e->data[e->slen + off];
      assert(pc->opcode == expected[n].codes[j].opcode * 2);
      assert(pc->blen == expected[n].codes[j].blen);
      assert(pc->value == expected[n].codes[j].value);
      assert(pc->address == expected[n].codes[j].address);

      off += pc->blen;
      j += (pc->blen - 8) / 8;
    }

    n++;
    i += sizeof(t_cheathdr) + e->slen + e->codelen;
  }
  assert (n == sizeof(expected)/sizeof(expected[0]));
}

