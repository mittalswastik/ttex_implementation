#include "llvm/Transforms/Scalar/LoopSimplifyCFG.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/DomTreeUpdater.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionAliasAnalysis.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/Scalar/TtexGenerateLoopAnalysis.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/CGSCCPassManager.h"

using namespace llvm;

PreservedAnalyses TtexGenerateLoopAnalysis::run(Module &M, ModuleAnalysisManager &MA) {

    // ModuleAnalysisManager MAM;
    // MAM.registerPass([&] { return LoopAnalysis(); });

    for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
        Function &F = *func_iter;
        if(!F.isDeclaration()){
            errs()<<"Loop simplify for function: "<<F.getName()<<"\n";
            FunctionAnalysisManager &FAM = MA.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
            FAM.registerPass([&] { return DominatorTreeAnalysis(); });
            FAM.registerPass([&] { return LoopAnalysis(); });

            // Create a FunctionPassManager for the current function
            FunctionPassManager FPM;
            //FPM.addPass(LoopAnalysis());
            FPM.addPass(LoopSimplifyPass());
            // Run the FunctionPassManager with the FunctionAnalysisManager
            FPM.run(F, FAM);
            errs()<<"------------- checking after loop simplify -----------\n";
        }
    }
    
    return PreservedAnalyses::all();
}