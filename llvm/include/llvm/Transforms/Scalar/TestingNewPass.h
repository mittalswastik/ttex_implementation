#ifndef LLVM_TRANSFORMS_TESTING_TESTINGNEWPASS_H
#define LLVM_TRANSFORMS_TESTING_TESTINGNEWPASS_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class TestingNewPass : public PassInfoMixin<TestingNewPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace llvm

#endif //LLVM_TRANSFORMS_TESTING_TESTINGNEWPASS_H