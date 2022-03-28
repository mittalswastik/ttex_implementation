#include "llvm/Transforms/Scalar/Sample.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include <iostream>
#include <bits/stdc++.h>
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/ADT/iterator.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/Loads.h"
#include "llvm/Analysis/PtrUseVisitor.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/ConstantFolder.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"

using namespace llvm;


// namespace llvm {
//   void initializeHelloNewPMPassPass(PassRegistry&);
// } // end namespace llvm

// namespace {
//   struct HelloNewPMPass : public PassInfoMixin<HelloNewPMPass> {

//     initializeHelloNewPMPassPass(*PassRegistry::getPassRegistry());

//     PreservedAnalyses run(Function &F,
//                           FunctionAnalysisManager &FAM) {
//       if(F.hasName())
//         errs() << "Hello " << F.getName() << "\n";
//       return PreservedAnalyses::all();
//     }
//   };
// }

namespace {
  // Hello - The first implementation, without getAnalysisUsage.
  class  HelloNewPMPass : public FunctionPass {
  public:
    static char ID; // Pass identification, replacement for typeid
    HelloNewPMPass() : FunctionPass(ID) {
      initializeHelloNewPMPassPass(*PassRegistry::getPassRegistry());
    }
    
    bool runOnFunction(Function &F) override {
      if(F.hasName())
      errs() << "Hello ------------------------------- " << F.getName() << "\n";
    //return PreservedAnalyses::all();
    return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
      AU.addRequired<LoopInfoWrapperPass>();
    }
  };
} // end anonymous namespace

char HelloNewPMPass::ID = 0;

// extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
// llvmGetPassPluginInfo() {
//   return {
//     LLVM_PLUGIN_API_VERSION, "HelloNewPMPass", "v0.1",
//     [](PassBuilder &PB) {
//       PB.registerPipelineParsingCallback(
//         [](StringRef Name, FunctionPassManager &FPM,
//            ArrayRef<PassBuilder::PipelineElement>) {
//           if(Name == "hello-new-pm-pass"){
//             FPM.addPass(HelloNewPMPass());
//             return true;
//           }
//           return false;
//         }
//       );
//     }
//   };
// }

static RegisterPass<HelloNewPMPass> C("hellopm", "execute all pass operations");

// static RegisterStandardPasses Y(
//     PassManagerBuilder::EP_EarlyAsPossible,
//     [](const PassManagerBuilder &Builder,
//        legacy::PassManagerBase &PM) { PM.add(new HelloNewPMPass()); });



INITIALIZE_PASS_BEGIN(HelloNewPMPass, "hellonewpmpass", 
                      "Some description for the Pass", 
                      false, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass) // Or whatever your Pass dependencies
INITIALIZE_PASS_END(HelloNewPMPass, "hellownewpmpass",
                    "Some description for the Pass", 
                    false, false)

FunctionPass* llvm::createHelloNewPMPass() {
  return new HelloNewPMPass();
}