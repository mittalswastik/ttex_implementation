#include "llvm/Transforms/Scalar/TestingNewPass.h"

using namespace llvm;

PreservedAnalyses TestingNewPass::run(Module &M,
                                      ModuleAnalysisManager &AM) {


  errs()<<"#################Executing the pass#######################"<<"\n";

  for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
      Function &F = *func_iter;
      errs()<<"Function Name New Pass: "<< F.getName() << "\n";
    }
  return PreservedAnalyses::all();
}

// test module pass with loop simplify -- check if loop information is available before module pass