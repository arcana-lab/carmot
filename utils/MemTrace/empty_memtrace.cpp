/*
 * Copyright 2002-2019 Intel Corporation.
 * 
 * This software is provided to you as Sample Source Code as defined in the accompanying
 * End User License Agreement for the Intel(R) Software Development Products ("Agreement")
 * section 1.L.
 * 
 * This software and the related documents are provided as is, with no express or implied
 * warranties, other than those that are expressly stated in the License.
 */

/*
 * Collect an address trace
 *
 * During execution of the program, record values in a MLOG and use the
 * MLOG values to reconstuct the trace. The MLOG can contain the actual
 * addresses or register values that can be used to compute the
 * address. This tool is thread safe. Each thread writes to its own MLOG
 * and each MLOG is dumped to a separate file.
 *
 */

/*
 * We do TRACE based instrumentation. At the top of the TRACE, we allocate
 * the maximum amount of space that might be needed for this trace in the
 * log. At each memory instruction, we just record the address at a
 * pre-determined slot in the log. If there are early exits from a trace,
 * then their slots will be empty. We initialize all the empty slots to an
 * invalid address so we can tell later that this instruction did not
 * reference memory.
 *
 * We check if the log is full at the top of the trace. If it is full, we
 * empty the log and reset the log pointer.
 *
 */
#include <cassert>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <list>
#include <unistd.h>
#include "pin.H"
#include "../../include/pin_interface.hpp"
using std::ofstream;
using std::vector;
using std::hex;
using std::string;
using std::endl;

int main(int argc, char * argv[])
{
  PIN_Init(argc, argv);

 PIN_StartProgram();

  return 0;
}

