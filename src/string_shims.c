#include <stddef.h>

size_t strlen(const char* s) {
  const char* p = s;
  while (*p) ++p;
  return (size_t)(p - s);
}

char* strchr(const char* s, int c) {
  char ch = (char)c;
  while (*s) {
    if (*s == ch) return (char*)s;
    ++s;
  }
  return ch == 0 ? (char*)s : 0;
}
