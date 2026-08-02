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

int strcmp(const char* first, const char* second) {
  while (*first && *first == *second) { ++first; ++second; }
  return (unsigned char)*first - (unsigned char)*second;
}

void* memchr(const void* data, int c, size_t count) {
  const unsigned char* p = (const unsigned char*)data;
  const unsigned char value = (unsigned char)c;
  while (count--) { if (*p == value) return (void*)p; ++p; }
  return 0;
}
