#pragma once

#define _GNU_SOURCE

#include <dlfcn.h>
#include <iostream>
#include "wrapper.hpp"

typedef void* (*real_realloc_t)(void*, size_t);
typedef void* (*real_calloc_t)(size_t, size_t);
typedef void* (*real_malloc_t)(size_t);

extern real_realloc_t real_realloc_ptr;
extern real_calloc_t real_calloc_ptr;
extern real_malloc_t real_malloc_ptr;
