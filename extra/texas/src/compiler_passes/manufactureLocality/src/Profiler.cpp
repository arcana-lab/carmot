

#include "profiler.hpp"



namespace {

    class SCEV_CARAT_Visitor : public SCEVVisitor<SCEV_CARAT_Visitor, Value*> {
        public:
            Value * visitConstant (const SCEVConstant *S) {
                return S->getValue();
            }
    };

    struct CAT : public ModulePass {
        static char ID; 

        CAT() : ModulePass(ID) {}

        bool doInitialization (Module &M) override {
            return false;
        }


        //This pass should go through all the functions and wrap
        //the memory instructions with the injected calls needed.	
        bool runOnModule (Module &M) override {

            bool modified = false;


            std::vector<Instruction *> returns;
            //Map that will map Loops to vectors for the stores and 
            std::unordered_map<std::string, int> functionCalls;
            populateLibCallMap(&functionCalls);

            Instruction* firstInst;

            //Get all exit points to report threads
            Function* main = M.getFunction("main");
            for(auto& B : *main){
                for(auto& I : B){
                    if(isa<ReturnInst>(I)){
                        returns.push_back(&I);
                    }
                }
            }            



            //Build the needed parts for making a callinst
            LLVMContext &TheContext = M.getContext();
            Type* voidType = Type::getVoidTy(TheContext);
            Type* voidPointerType = Type::getInt8PtrTy(TheContext, 0);
            Type* int64Type = Type::getInt64Ty(TheContext);
            //Dealing with printing states at end of run
            if(returns.size() > 0){
                auto signature = FunctionType::get(voidType,false); 
                auto allocFunc = M.getOrInsertFunction(CARAT_STATE_OPT, signature);
                for(auto* RI : returns){               
                    const Twine &NameStr = "";
                    CallInst* addToAllocationTable = CallInst::Create(allocFunc, NameStr, RI);
                    modified = true;
                }
            }

            
        }
    

        void getAnalysisUsage (AnalysisUsage &AU) const override {
            AU.addRequired<AssumptionCacheTracker>();
            AU.addRequired<DominatorTreeWrapperPass>();
            AU.addRequired<LoopInfoWrapperPass>();
            AU.addRequired<ScalarEvolutionWrapperPass>();
            return ;
        }
    };


    // Next there is code to register your pass to "opt"
    char CAT::ID = 0;
    static RegisterPass<CAT> X("ManufactureLocality", "Attempt to manufacture locality");

    // Next there is code to register your pass to "clang"
    static CAT * _PassMaker = NULL;
    static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
            [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
            if(!_PassMaker){ PM.add(_PassMaker = new CAT());}}); // ** for -Ox
    static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
            [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
            if(!_PassMaker){ PM.add(_PassMaker = new CAT());}}); // ** for -O0




} //End namespace
