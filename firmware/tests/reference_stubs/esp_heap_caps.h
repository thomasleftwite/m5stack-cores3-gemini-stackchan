#pragma once
#include <stdlib.h>
enum { MALLOC_CAP_8BIT=1, MALLOC_CAP_SPIRAM=2 };
inline void* heap_caps_malloc(size_t n,unsigned) { return malloc(n); }
inline void* heap_caps_calloc(size_t n,size_t s,unsigned) { return calloc(n,s); }
inline void heap_caps_free(void* p) { free(p); }
