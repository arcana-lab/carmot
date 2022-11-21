#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../../../include/wrapper.hpp"

int main(void) {
  size_t pagesize = getpagesize();

  printf("System page size: %zu bytes\n", pagesize);

  char * region = (char*) mmap(
    (void*) (pagesize * (1 << 20)),   // Map from the start of the 2^20th page
    pagesize,                         // for one page length
    PROT_READ|PROT_WRITE|PROT_EXEC,
    MAP_ANON|MAP_PRIVATE,             // to a private block of hardware memory
    0,
    0
  );
  if (region == MAP_FAILED) {
    perror("Could not mmap");
    return 1;
  }

  uint64_t stateID = caratGetStateWrapper("main", 0);
  strcpy(region, "Hello, poftut.com");
  caratReportStateWrapper(stateID);

  printf("Contents of region: %s\n", region);

  int unmap_result = munmap(region, 1 << 10);
  if (unmap_result != 0) {
    perror("Could not munmap");
    return 1;
  }

  return 0;
}
