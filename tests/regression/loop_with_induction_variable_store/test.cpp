#include <cstdint>
#include <stdlib.h>
#include <stdio.h>
#include "../../../include/wrapper.hpp"

#define SIZE 10

int* f(int *a, int it){
  for (auto i = 1 ; i < it; ++i){
    a[i-1] = 42;
    printf("%d\n", a[i]);
  }

  return a;
}

int main (int argc, char *argv[]){
  int a[SIZE];
  int *b;

  for (auto i = 1 ; i < SIZE; ++i){
    uint64_t stateID = caratGetStateWrapper("main", 0);
    b = f(a, SIZE);
    caratReportStateWrapper(stateID);
  }

  printf("%d\n", *b);

  return 0;
}
