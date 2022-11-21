
#include "Utils.hpp"

using namespace llvm;

class StateHandler
{
public:
    StateHandler(Module *M,
                 std::unordered_map<std::string, int> *FunctionMap,
                 uint64_t tT);

    // Injection methods
    void Inject();

private:
    // Initial state
    Module *M;
    std::unordered_map<std::string, int> *FunctionMap;
    uint64_t temporalTracking = 0;

    // Analyzed state
    std::vector<Instruction *> MemUses;
    std::vector<Instruction *> mainReturns;

    // Private methods
    void _getAllNecessaryInstructions();
    void _buildNecessaryFunctions();
};

