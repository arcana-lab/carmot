#pragma once

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

EXTERNC uint64_t caratGetStateWrapper(char *funcName, uint64_t lineNum);
EXTERNC void caratReportStateWrapper(uint64_t stateID);
EXTERNC void endStateInvocationWrapper(uint64_t stateID);
EXTERNC void FindCycleInAllocationMapWrapper(void);
