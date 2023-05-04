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
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/PassManager.h"
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
#include "llvm/Transforms/Scalar/TtexLoadToPhi.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"

using namespace llvm;

static cl::opt<bool> MyOption("loadtophi",
  cl::desc("Description of my custom option"),
  cl::init(false));

std::vector<Instruction*> getInductionVariable(Loop *L) {
  // Get the header block of the loop
  BasicBlock *header = L->getHeader();

  // Look for a variable that is loaded at the beginning of the loop
  // and stored at the end of the loop with an increment or decrement operation
  //Instruction *loadInstr = nullptr;
  std::vector<Instruction*> loadInstr; 
  std::vector<Instruction*> storeInstr;
  for(llvm::BasicBlock::iterator I_iter = header->begin(), Iend = header->end(); I_iter != Iend ; ++I_iter){
    Instruction &I = *I_iter;
    if (auto *load = dyn_cast<LoadInst>(&I)) {
      errs()<<"---------- found a load instruction ----------------\n";
      loadInstr.push_back(load);
    }
  }

  if (loadInstr.size() == 0) {
    errs()<<"---- no load instruction -----\n";
    return loadInstr;
  }

  std::vector<int> index;

  for(int i=0 ; i < loadInstr.size() ; i++){
    for(llvm::BasicBlock::iterator I_iter = L->getLoopLatch()->begin(), Iend = L->getLoopLatch()->end(); I_iter != Iend ; ++I_iter){
      Instruction &I = *I_iter;
      if (auto *store = dyn_cast<StoreInst>(&I)) {
        if (store->getPointerOperand() == loadInstr[i]->getOperand(0)){
          storeInstr.push_back(store);
          errs()<<"---------------- found corresponding store inst ---------------\n";
          break;
        }
      }
    }

    if(storeInstr.size() < i+1){
      errs()<<"----- corresponding store not found ------\n";
      loadInstr.erase(loadInstr.begin()+i); // remove the loadInst without corresponding store in the latch block
    }

    // this eliminates all load instructions for upper bound as they would not have store instructions as upper 
    // values do not change    
  }

  // Check that the variable is not modified inside the loop except for the increment or decrement operation
  bool check = false;
  for(int i = 0 ; i < loadInstr.size() ; i++){
    for(llvm::BasicBlock::iterator I_iter = L->getLoopLatch()->begin(), Iend = L->getLoopLatch()->end(); I_iter != Iend ; ++I_iter){
      Instruction &I = *I_iter;
      if(I.getOpcode() == Instruction::Add || I.getOpcode() == Instruction::Sub){
        LoadInst *ltemp = dyn_cast<LoadInst>(loadInstr[i]);
        errs()<<ltemp<<"\n";
        if(LoadInst *ltemp_2 = dyn_cast<LoadInst>(I.getOperand(0))){
          errs()<<"------ get operand(0) is a load inst ------\n";
          if(ltemp->getPointerOperand() == ltemp_2->getPointerOperand()){
            errs()<<"------ pointer operand are the same ------\n";
            if(ConstantInt *Itemp = dyn_cast<ConstantInt>(I.getOperand(0))){
              errs()<<"---- constant add found -----\n";
              check = true;
              break;
            }
          }
        }
      }
    }  
      
    if(!check){
      errs()<<"---- load not an induction variable for load Inst ----"<<loadInstr[i]<<"\n";
      loadInstr.erase(loadInstr.begin()+i);
    }

    check = false;
  }

  // The variable is an induction variable
  for(int i = 0 ; i < loadInstr.size() ; i++){
    errs() << "############## Found induction variable: #########" << *loadInstr[i] << "\n";
  }
  return loadInstr;
}

PreservedAnalyses TtexLoadToPhiPass::run(Module &M, ModuleAnalysisManager &MA) {

  errs()<<"----------------- ttex load to phi found-------------\n";


  for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
    Function &F = *func_iter;

    if(!F.isDeclaration()){
      auto &FM = MA.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
      //FM.registerPass(DominatorTreeAnalysis());
      FM.registerPass([]() { return llvm::DominatorTreeAnalysis(); });
      FunctionPassManager FPM;
      //FPM.run(createLoopSimplifyPass());
      // FPM.addPass(createLoopSimplifyPass());

      if(!FM.empty()){
        errs()<<"---- Is function analysis manager empty ------- for Function: "<<F.getName()<<"\n";
        //errs()<<FAM.getResult<LoopAnalysis>(F);
      }

      //if(F.getName() != "_ZNSt8ios_base4InitD1Ev" && F.getName() != "_ZNSt8ios_base4InitC1Ev" && F.getName() != "__cxx_global_var_init" && F.getName() != "__cxa_atexit"){

        DominatorTree* DT = &FM.getResult<DominatorTreeAnalysis>(F);
        //DominatorTree DT = llvm::DominatorTree();
        //DT.recalculate(F);
        errs()<<"We get the dominator tree\n";
        // DT->recalculate(F);
        LoopInfoBase<BasicBlock, Loop>* LIB = new llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>();
        // //LIB->releaseMemory();
        LIB->analyze(*DT);

        if(LIB){
          if(LIB->begin() == LIB->end()){
            errs()<<"no loop info\n";
          }

          // else {
          //   errs()<<"\n";
          // }

          else {
            FM.invalidate(F,PreservedAnalyses::none());
            LoopInfo *LI = &FM.getResult<LoopAnalysis>(F);
            DT = &FM.getResult<DominatorTreeAnalysis>(F);
            //auto &AC = FM.getResult<AssumptionAnalysis>(F);
            auto &ORE = FM.getResult<OptimizationRemarkEmitterAnalysis>(F);
            ScalarEvolution *SE = FM.getCachedResult<ScalarEvolutionAnalysis>(F);
            AssumptionCache *AC = &FM.getResult<AssumptionAnalysis>(F);
            auto *MSSAAnalysis = FM.getCachedResult<MemorySSAAnalysis>(F);
            std::unique_ptr<MemorySSAUpdater> MSSAU;
            if (MSSAAnalysis) {
              auto *MSSA = &MSSAAnalysis->getMSSA();
              MSSAU = std::make_unique<MemorySSAUpdater>(MSSA);
            }
            errs()<<"--------------- Loop details evaluated ------------\n";
          
            for (Loop *L : *LI) {

              simplifyLoop(L, DT, LI, SE, AC, MSSAU.get(), /*PreserveLCSSA*/ false);
              formLCSSARecursively(*L, *DT, LI, SE);

              //for(LoopInfo::iterator loop_iter = LIB->begin(), loop_iter_end = LIB->end(); loop_iter != loop_iter_end; ++loop_iter){ 
            //for (auto *ltemp :
                errs()<<"--------------- found a loop to evaluate load instruction ------------\n";
                //Loop *L = *loop_iter;
                //Instruction *I = nullptr;
                std::vector<Instruction *> I = getInductionVariable(L);

                for(int i = 0 ; i < I.size() ; i++){
                  errs()<<"----------- replace load with phi------------\n";
                  I[i]->replaceAllUsesWith(PHINode::Create(I[i]->getType(),0,"",I[i])); // create a phi node replacement here -- this would help reducing the issue with load and store
                }
            }
          
            PreservedAnalyses PA;
            PA.preserve<DominatorTreeAnalysis>();
            PA.preserve<LoopAnalysis>();
            PA.preserve<ScalarEvolutionAnalysis>();
            PA.preserve<DependenceAnalysis>();
            if (MSSAAnalysis)
              PA.preserve<MemorySSAAnalysis>();
            // BPI maps conditional terminators to probabilities, LoopSimplify can insert
            // blocks, but it does so only by splitting existing blocks and edges. This
            // results in the interesting property that all new terminators inserted are
            // unconditional branches which do not appear in BPI. All deletions are
            // handled via ValueHandle callbacks w/in BPI.
            PA.preserve<BranchProbabilityAnalysis>();
            return PA;

          }
        }
      }
    }

  return PreservedAnalyses::all();

}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "TtexLoadToPhiPass", "v0.1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef PassName, ModulePassManager &MPM, ...) {
          if(PassName == "loadtophi"){
            MPM.addPass(TtexLoadToPhiPass());
            return true;
          }
          return false;
        }
      );
    }
  };
}