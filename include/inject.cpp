#include "inject.hpp"

real_realloc_t real_realloc_ptr = ((real_realloc_t) dlsym(RTLD_NEXT, "realloc"));
real_calloc_t real_calloc_ptr = ((real_calloc_t) dlsym(RTLD_NEXT, "calloc"));
real_malloc_t real_malloc_ptr = ((real_malloc_t) dlsym(RTLD_NEXT, "malloc"));


// REALLOC
void *real_realloc(void *ptr, size_t new_size){
  return real_realloc_ptr(ptr, new_size);
}

void* realloc(void *ptr, size_t new_size) {
  // Perform the actual system call
  void *res = real_realloc(ptr, new_size);

  // Call runtime (with checks)
  caratStateReallocWrapperLibrary(ptr, res, new_size, (char*)"dontcare", (char*)"dontcarefile", 0, 0);

  return res;
}

// CALLOC
void *real_calloc(size_t num, size_t size){
  return real_calloc_ptr(num, size);
}

void* calloc(size_t num, size_t size) {
  // Perform the actual system call
  void *res = real_calloc(num, size);

  // Call runtime (with checks)
  caratStateCallocWrapperLibrary(res, num, size, (char*)"dontcare", (char*)"dontcarefile", 0, 0);

  return res;
}

// MALLOC
void* real_malloc(size_t size) {
  return real_malloc_ptr(size);
}

void* malloc(size_t size) {
  // Perform the actual system call
  void *res = real_malloc(size);

  // Call runtime (with checks)
  caratStateAllocWrapperLibrary(res, size, (char*)"dontcare", (char*)"dontcarefile", 0, 0);

  return res;
}
