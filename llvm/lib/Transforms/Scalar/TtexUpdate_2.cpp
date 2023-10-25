#include "llvm/Transforms/Scalar/LoopUnrollPass.h"
#include "llvm/IR/Argument.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/None.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/CodeMetrics.h"
#include "llvm/Analysis/LazyBlockFrequencyInfo.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/LoopUnrollAnalyzer.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Scalar/TtexUpdate_2.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/LoopPeel.h"
#include "llvm/Transforms/Utils/LoopSimplify.h"
#include "llvm/Transforms/Utils/FixIrreducible.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/SizeOpts.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Attributes.inc"
#include <iostream>
#include <bits/stdc++.h>
#include "llvm/IR/DataLayout.h"
#include "llvm/Transforms/Scalar/LoopSimplifyCFG.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/DomTreeUpdater.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/ScalarEvolutionAliasAnalysis.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;

// namespace llvm {
//   void initializeTtexPassPass (PassRegistry&);
// } // end namespace llvm

static cl::opt<bool> Myfile("check_file",
  cl::desc("read update from file"),
  cl::init(false)); 

bool check = false;

long int secure_wcet_ns = 9000000000;
int loop_unique_id = 0;

typedef struct loop_details_pass {
  int parallel_id;
  int loop_id;
  int split_factor;
  int unique_loop_id;
  int seq_split;
  int total_inst;
  long int wcet_ns;
} loop_details_pass;

typedef struct para_details {
  int parallel_id;
  int id;
  int ref;
  int seq_split;
  int total_inst;
  long int wcet_ns;
} para_details;

std::unordered_map<Loop*, int> secure_loops;
std::unordered_map<int, loop_details_pass> secure_loops_2;
// std::unordered_map<> // how to define map for code regions - they are not like loops or functions

std::vector< std::vector<loop_details_pass> > loop_details_profiler;
std::vector< std::vector<para_details> > region_details_profiler;

// std::vector< std::vector<loop_details_pass> > loop_details_from_profiler;
// std::vector< std::vector<para_details> > region_details_from_profiler;

int loop_counter_2 = 0;

bool maxvuln_set_2 = true;

#define omp_for_ref -1
#define omp_sections_ref 0
#define omp_single_ref -2

void AddFunction(llvm::Module* M, int parallel_id, int sub_id, int loop_id, BasicBlock *block, Instruction *Inst){
  llvm::LLVMContext &CTX = M->getContext();

  FunctionType *testing = FunctionType::get(
      Type::getVoidTy(CTX),
      {IntegerType::getInt32Ty(CTX), IntegerType::getInt32Ty(CTX), IntegerType::getInt32Ty(CTX),},
      //PointerType::getPointerAddressSpace(),
      /*IsVarArgs=*/false);

  FunctionCallee hookTest = M->getOrInsertFunction("ompt_test", testing);
  if (Value *calleeFunction = hookTest.getCallee()) {
    if(Function* Fn = dyn_cast<Function>(calleeFunction)) {
      Fn->addFnAttr(Attribute::NoInline);
    }
  }

  Function *add_timer_calls = M->getFunction("ompt_test");
  // hookTest.addFnAttr(Attribute::NoInline);
  //Function *hook = dyn_cast<Function>(hookTest.getCallee());
  std::vector<Value*> args;
  ConstantInt *arg1 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),parallel_id, false);
  ConstantInt *arg2 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),sub_id, false);
  ConstantInt *arg3 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),loop_id, false);
  args.push_back(arg1);
  args.push_back(arg2);
  args.push_back(arg3);
  //hookTest->addAttribute(hook
  //add_timer_calls->addFnAttr(Attribute::NoInline);
  //Instruction *calltemp = security_2.CreateCall(add_timer_calls,args);
  // DILocation *DebugLoc = calltemp->getDebugLoc();
  // security_2.SetCurrentDebugLocation(DebugLoc);
  Value *my_function = hookTest.getCallee();

  if(Function* fn_test = dyn_cast<Function>(my_function)){
    errs() <<"----- a function returned ----\n";
  }

  //my_function->addAttribute(AttributeList::FunctionIndex, Attribute::NoInline);
  /* commenting 6 lines below for now*/
  CallInst *callinst;
  if(block == nullptr) {
    callinst = llvm::CallInst::Create(hookTest,args,"",Inst);
  }

  else {
    callinst = llvm::CallInst::Create(hookTest,args,"",block);
  }
  
  Function *fn_test_2 = callinst->getCalledFunction();
  std::vector<Attribute> attr_list;
  AttributeSet attr_set = AttributeSet::get(CTX, attr_list);
  fn_test_2->addFnAttr(Attribute::NoInline);
  fn_test_2->addFnAttr(Attribute::NoUnwind);

}

bool LoopSplit_2(Loop *L, unsigned count, int parallel_id, int sub_id, int loop_id){

  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *LatchBlock = L->getLoopLatch();

  std::vector<BasicBlock *> OriginalLoopBlocks = L->getBlocks();

  bool no_latch = false;

  llvm::Function* Func = Header->getParent();
  llvm::Module *M = Func->getParent();
  llvm::LLVMContext &CTX = M->getContext();

  std::vector<BasicBlock*> PredecessorBlocks;
  SmallVector<BasicBlock*> AllLatches;
  //std::vector<BasicBlock*> *AllLatches;

  for(BasicBlock *B: predecessors(Header)){
    PredecessorBlocks.push_back(B);
  }

  if(PredecessorBlocks.size() == 0){
    errs()<<"no predecessor block found\n";
    // there can be zero predecessors but let's go with an exeception here and add another entry point to a function than just starting with a loop
    return false;
  }

  if(LatchBlock == nullptr){
    L->getLoopLatches(AllLatches);
    if(AllLatches.size() == 0){
      errs()<<"no latch block found\n";
      return false;
      //no_latch = true; // header is the exiting blocks as well - back edge to itself
    }
  }

  PHINode* counter_val = PHINode::Create(llvm::Type::getInt32Ty(CTX), 3, "phi",&Header->front());

  llvm::BasicBlock *Secure_1 = BasicBlock::Create(CTX, "TtexSecure_1", Func);
  llvm::BasicBlock *Secure_2 = BasicBlock::Create(CTX, "TtexSecure_2", Func);

  llvm::ConstantInt *secure_counter;
  llvm::ConstantInt *compare_to_zero;
  IRBuilder<> security(Secure_1);
  PHINode *phisecure;
  IRBuilder<> security_2(Secure_2);

  AddFunction(M,parallel_id, sub_id,loop_id,Secure_2,nullptr);

  //attr_set.addAttribute(CTX, AttributeSet::FunctionIndex, Attribute::NoInline);
  //fn_test_2->addAttributes(0, AttributeSet::get(c, AttributeSet::FunctionIndex, attr));
  //security_2.CreateBr(Header);
  llvm::BranchInst::Create(Header,Secure_2); 

  Value* ctr_val = security.CreateNSWAdd(counter_val, llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),1, false), "");  
  Value* temp = security.CreateSRem(counter_val, llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),count, false));
  compare_to_zero = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),0,false);
  Value* compare = security.CreateICmpEQ(temp,compare_to_zero);


  for(int i = 0 ; i < PredecessorBlocks.size() ; i++){ // latch block will no longer be the predecessor block
    if(LatchBlock == nullptr){
      for(int i = 0 ; i < AllLatches.size() ; i++){
        BasicBlock *LatchBlock_temp = AllLatches[i];
        if(PredecessorBlocks[i] != LatchBlock_temp){
          counter_val->addIncoming(llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),0, false),PredecessorBlocks[i]); 
        }
      } 
    }

    else {
      if(PredecessorBlocks[i] != LatchBlock){
        counter_val->addIncoming(llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),0, false),PredecessorBlocks[i]);
      }
    }
  }

  counter_val->addIncoming(ctr_val,Secure_1);
  counter_val->addIncoming(ctr_val,Secure_2);

  security.CreateCondBr(compare, Secure_2, Header);

  if(LatchBlock == nullptr){
    for(int i = 0 ; i < AllLatches.size() ; i++){
      BasicBlock *LatchBlock_temp = AllLatches[i];
      llvm::Instruction *Linst = LatchBlock_temp->getTerminator();
      if(BranchInst *BI = dyn_cast<BranchInst>(Linst)){
        llvm::BasicBlock * ExitMain = BI->getSuccessor(0);
        if(BI->isConditional()) {
          if(ExitMain == Header){
            ExitMain = BI->getSuccessor(1);
            Value *conditionValue = BI->getCondition();
            Linst->eraseFromParent();
            llvm::BranchInst::Create(Secure_1,ExitMain,conditionValue,LatchBlock_temp);
          }

          else {
            Value *conditionValue = BI->getCondition();
            Linst->eraseFromParent();
            llvm::BranchInst::Create(ExitMain,Secure_1,conditionValue,LatchBlock_temp);
          }
        }

        else { // case: if header has the exit condition - that header is the exiting block then latch only has a branch 
          Linst->eraseFromParent();
          llvm::BranchInst::Create(Secure_1,LatchBlock_temp);
        }
      }
    }
  }

  else {
    
    llvm::Instruction *Linst = LatchBlock->getTerminator();
    if(BranchInst *BI = dyn_cast<BranchInst>(LatchBlock->getTerminator())){
      llvm::BasicBlock * ExitMain = BI->getSuccessor(0);
      if(BI->isConditional()) {
        if(ExitMain == Header){
          ExitMain = BI->getSuccessor(1);
          Value *conditionValue = BI->getCondition();
          Linst->eraseFromParent();
          llvm::BranchInst::Create(Secure_1,ExitMain,conditionValue,LatchBlock);
        }

        else {
          Value *conditionValue = BI->getCondition();
          Linst->eraseFromParent();
          llvm::BranchInst::Create(ExitMain,Secure_1,conditionValue,LatchBlock);
        }
      }

      else { // case: if header has the exit condition - that header is the exiting block then latch only has a branch 
        Linst->eraseFromParent();
        llvm::BranchInst::Create(Secure_1,LatchBlock);
      }
    }
  }
  

  for(llvm::BasicBlock::iterator I = Header->begin(), Iend = Header->end(); I != Iend ; ++I){
    Instruction &Inst = *I;
    if(PHINode *Temp = dyn_cast<PHINode>(I)){
      if(Temp != counter_val){
        for (unsigned i = 0; i < Temp->getNumIncomingValues(); ++i) {
          if(LatchBlock == nullptr) {
            for(int j = 0 ; j < AllLatches.size() ; j++){
              BasicBlock *LatchBlock_temp = AllLatches[j];
              if(Temp->getIncomingBlock(i) == LatchBlock_temp){
                Temp->setIncomingBlock(i,Secure_1);
                Temp->addIncoming(Temp->getIncomingValue(i),Secure_2);
              }
            }
          }

          else {
            if(Temp->getIncomingBlock(i) == LatchBlock){
              Temp->setIncomingBlock(i,Secure_1);
              Temp->addIncoming(Temp->getIncomingValue(i),Secure_2);
            }
          }
        }
      }
    }
  }
  
  return true;
}

int returnInstCount(Function *F, ModuleAnalysisManager &MA, int parallel_region_id, int sub_id, int loop_id, int split_id, int count){
  llvm::Module *M = F->getParent();
  llvm::LLVMContext &CTX = M->getContext();
  auto &FM = MA.getResult<FunctionAnalysisManagerModuleProxy>(*M).getManager();
  FM.invalidate(*F,PreservedAnalyses::none());
  LoopInfo *LI = &FM.getResult<LoopAnalysis>(*F);

    for (BasicBlock &BB : *F) {

      // break if basic block is a part of a loop in that function
      Loop *temp_loop = LI->getLoopFor(&BB); // will give null for inner loops?
      if(temp_loop) {
        continue; // not handling other loops (Will be handled on their call)
      }

      for (Instruction &I : BB) {

        if(count == split_id) {
            AddFunction(M,parallel_region_id,sub_id,loop_id,nullptr,&I);
        }

        if (CallInst *CI = dyn_cast<CallInst>(&I)) {
          Function *CalledFunc = CI->getCalledFunction();
          if (CalledFunc && !CalledFunc->isDeclaration()) {
            if(!CalledFunc->getName().contains(".omp_outlined.")){
              if(CalledFunc->getName() != F->getName()){
                count = returnInstCount(CalledFunc, MA, parallel_region_id, sub_id, loop_id, split_id, count);
              }
            }
          }
        }

        else if (InvokeInst *CI= dyn_cast<InvokeInst>(&I)){
          Function *CalledFunc = CI->getCalledFunction();
          if (CalledFunc && !CalledFunc->isDeclaration()) {
            if(!CalledFunc->getName().contains(".omp_outlined.")){
              if(CalledFunc->getName() != F->getName()){
                count = returnInstCount(CalledFunc, MA, parallel_region_id, sub_id, loop_id, split_id, count);
              }
            }
          }
        }

        count += 1;
      }
    }  

  return count;
}

int addSeqCallsInLoop(Function &F, Loop *L, int parallel_region_id, int sub_id, int loop_id, int split_id, ModuleAnalysisManager &MA){
  int count = 0;
  std::vector<BasicBlock *> OriginalLoopBlocks = L->getBlocks();

  llvm::Module *M = F.getParent();
  llvm::LLVMContext &CTX = M->getContext();
  auto &FM = MA.getResult<FunctionAnalysisManagerModuleProxy>(*M).getManager();
  FM.invalidate(F,PreservedAnalyses::none());
  LoopInfo *LI = &FM.getResult<LoopAnalysis>(F);

  for(int i = 0 ; i < OriginalLoopBlocks.size() ; i++){
    BasicBlock *LoopBlock = OriginalLoopBlocks[i];

    Loop *temp_loop = LI->getLoopFor(LoopBlock);
    if(temp_loop && temp_loop != L) {
      continue;
    }

    for(llvm::BasicBlock::iterator I = LoopBlock->begin(), Iend = LoopBlock->end(); I != Iend ; ++I){
      Instruction *Inst = &*I;
      
      if(count == split_id) {
          AddFunction(M,parallel_region_id,-1,loop_id,nullptr,Inst);
      }
      
      if (CallInst *CI = dyn_cast<CallInst>(Inst)) {
        // It's a call instruction
        Function *CalledFunc = CI->getCalledFunction();
        if (CalledFunc && !CalledFunc->isDeclaration()) {
          if(!CalledFunc->getName().contains(".omp_outlined.")){
            if(CalledFunc->getName() != F.getName()){ // eliminate recursive calls
              count = returnInstCount(CalledFunc, MA, parallel_region_id, sub_id, loop_id, split_id, count);
            }
          }
        }
      }

      else if (InvokeInst *CI= dyn_cast<InvokeInst>(Inst)){
        // It's a call instruction
        Function *CalledFunc = CI->getCalledFunction();
        if (CalledFunc && !CalledFunc->isDeclaration()) {
          if(!CalledFunc->getName().contains(".omp_outlined.")){
            if(CalledFunc->getName() != F.getName()){ // eliminate recursive calls
              count = returnInstCount(CalledFunc, MA, parallel_region_id, sub_id, loop_id, split_id, count);
            }
          }
        }
      }

      // other instructions
      count += 1; 
    }
  }

  return count;
}


std::vector<Loop*> generateAllLoops_2(std::vector<Loop*> allLoops_2, Loop *L) { // gets outer loop
  if(L->getSubLoops().size() == 0){
    allLoops_2.push_back(L);
    return allLoops_2;
  }

  std::vector<Loop*> temp = L->getSubLoops();
  for(int i = 0 ; i < temp.size() ; i++){
    allLoops_2 = generateAllLoops_2(allLoops_2, temp[i]);
  }

  allLoops_2.push_back(L);
  return allLoops_2;
}


std::vector<Loop*> retrieveLoopsFunc(Function &F, ModuleAnalysisManager &MA){
  
  llvm::Module *M = F.getParent();
  llvm::LLVMContext &CTX = M->getContext();
  std::vector<Loop*> allLoops_2;
  auto &FM = MA.getResult<FunctionAnalysisManagerModuleProxy>(*M).getManager();
  FunctionPassManager FPM;
  DominatorTree* DT;// = &FM.getResult<DominatorTreeAnalysis>(F);
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

  errs()<<"checking for errors"<<"\n";

  for (Loop *ltemp : *LI) { // gives all the outer loops
    allLoops_2 = generateAllLoops_2(allLoops_2, ltemp);
  }

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
        if (CallInst *CI = dyn_cast<CallInst>(&I)) {
            // It's a call instruction
            Function *CalledFunc = CI->getCalledFunction();
            errs()<<"Function name for loop split is:"<<CalledFunc->getName()<<"\n";
            if (CalledFunc && !CalledFunc->isDeclaration()) {
              errs()<<"Function is not a declaration\n";
              if(!CalledFunc->getName().contains(".omp_outlined.")){
                if(CalledFunc->getName() != F.getName()){ // eliminate recursive calls
                  errs()<<"Function is also not omp_outlined\n";
                  std::vector<Loop*> temp_loops = retrieveLoopsFunc(*CalledFunc,MA);
                  allLoops_2.insert(allLoops_2.end(),temp_loops.begin(),temp_loops.end());
                }
              }
            } else {
                // Indirect function call, handle accordingly
            }
        }

        else if (InvokeInst *CI= dyn_cast<InvokeInst>(&I)){
          // It's a call instruction
            Function *CalledFunc = CI->getCalledFunction();
            errs()<<"Function name for loop split is:"<<CalledFunc->getName()<<"\n";
            if (CalledFunc && !CalledFunc->isDeclaration()) {
              errs()<<"Function is not a declaration\n";
              if(!CalledFunc->getName().contains(".omp_outlined.")){
                errs()<<"Function is also not omp_outlined\n";
                std::vector<Loop*> temp_loops = retrieveLoopsFunc(*CalledFunc,MA);
                allLoops_2.insert(allLoops_2.end(),temp_loops.begin(),temp_loops.end());
              }
            } else {
                // Indirect function call, handle accordingly
            }
        }
    }
  }

  //testing
  //Loop *ltemp = *loop_iter;

  // for(int i = 0 ; i < allLoops_2.size(); i++) {
  //   Loop* ltemp = allLoops_2[i];
  //   //simplifyLoop(ltemp, DT, LI, SE, AC, MSSAU.get(), /*PreserveLCSSA*/ false);
  //   // formLCSSARecursively(*ltemp, *DT, LI, SE);
  //   // formLCSSARecursively(*ltemp, *DT, LI, SE);
  // }

  return allLoops_2; // return
}

// void splittingLoopsPhase2(Function &F, int parallel_region_id){
//   for(int i = 0 ; i < loop_details_profiler[parallel_region_id].size(); i++) {
//     std::pair<int,int> test = std::make_pair(parallel_region_id, loop_details_profiler[parallel_region_id][i].loop_id);
//     Loop* ltemp = secure_loops_2[test];
//     if(ltemp->isInvalid()){
//       continue;
//     }



//   }  
// }

void splittingLoops(Function &F, int parallel_region_id, ModuleAnalysisManager &MA){
  
  loop_counter_2 = 0;

  llvm::Module *M = F.getParent();
  llvm::LLVMContext &CTX = M->getContext();
  std::vector<Loop*> allLoops_2 = retrieveLoopsFunc(F,MA);
  auto &Options = cl::getRegisteredOptions();
  std::vector<loop_details_pass> loop_data;
  for(int i = 0 ; i < allLoops_2.size(); i++) {
    Loop* ltemp = allLoops_2[i];
    if(ltemp->isInvalid()){
      continue;
    }
    BasicBlock *header = ltemp->getHeader();

    BasicBlock *Preheader = ltemp->getLoopPreheader();
    //BasicBlock *Header = ltemp->getHeader();
    BasicBlock *LatchBlock = ltemp->getLoopLatch();
    BasicBlock *ExitingBlock = ltemp->getExitingBlock();
    BasicBlock *ExitBlock = ltemp->getExitBlock();
    std::string ph_label, h_label, eg_label, l_label, ex_label;
    raw_string_ostream stream1(ph_label), stream2(h_label), stream3(eg_label), stream4(l_label), stream5(ex_label);
    if(Preheader){
      errs()<<"preheader found"<<"\n";
      Preheader->printAsOperand(stream1,false);
      errs()<<ph_label<<"\n";
    }
    // printAsOperand is the parent class Value function storing in output stream
    if(header){
      errs()<<"header found"<<"\n";
      header->printAsOperand(stream2,false);
      errs()<<h_label<<"\n";
    }
    
    if(ExitingBlock){
      errs()<<"exiting found"<<"\n";
      ExitingBlock->printAsOperand(stream3,false);
      errs()<<eg_label<<"\n";
    }

    if(LatchBlock){
      errs()<<"LatchBlock found"<<"\n";
      LatchBlock->printAsOperand(stream4,false);
      errs()<<l_label<<"\n";
    }

    if(ExitBlock){
      errs()<<"exitblock found"<<"\n";
      ExitBlock->printAsOperand(stream5,false);
      errs()<<ex_label<<"\n";
    }

    errs()<<"Found a loop"<<"\n";

    bool split_check = false;

    auto &Options = cl::getRegisteredOptions();
    if (!(Options.count("check_file") && Myfile)){
      if(secure_loops.find(ltemp) != secure_loops.end()){
        // repeated loops are checked here
        // secure_loops_2[loop_unique_id] = ltemp; // still need to add the details to other parallel region
        loop_details_pass lt;
        lt = secure_loops_2[secure_loops[ltemp]];
        lt.parallel_id = parallel_region_id; // don't need to change it?
        lt.loop_id = loop_counter_2;
        split_check = true;
        loop_data.push_back(lt);
        loop_counter_2++;
      }

      else {
        split_check = LoopSplit_2(ltemp, 2, parallel_region_id, -1, loop_counter_2);
        if(split_check){
          loop_details_pass lt;
          lt.parallel_id = parallel_region_id;
          lt.loop_id = loop_counter_2;
          lt.total_inst = addSeqCallsInLoop(F,ltemp,parallel_region_id,-1,loop_counter_2,-1,MA); // now add
          lt.seq_split = -1;
          lt.split_factor = 2;
          lt.unique_loop_id = loop_unique_id;
          lt.wcet_ns = 0;
          secure_loops[ltemp] = loop_unique_id; // loop has been secured - this map helps me check during sequential split (since no simplify two loops can share header having bulk code)
          //std::pair<int,int> test = std::make_pair(parallel_region_id,loop_counter_2);
          secure_loops_2[loop_unique_id] = lt;
          loop_data.push_back(lt);
          loop_counter_2++;
          loop_unique_id++;
        }
      } 

    loop_details_profiler[parallel_region_id] = loop_data;
    }

    else {  // this will change on the basis of new split analysis given wcet
      if(secure_loops.find(ltemp) != secure_loops.end()){
        // repeated loops are checked here
        // secure_loops_2[loop_unique_id] = ltemp; // still need to add the details to other parallel region
        loop_details_pass lt;
        lt = secure_loops_2[secure_loops[ltemp]];
        loop_details_profiler[parallel_region_id][loop_counter_2] = lt;
        loop_details_profiler[parallel_region_id][loop_counter_2].parallel_id = parallel_region_id;
        loop_details_profiler[parallel_region_id][loop_counter_2].loop_id = loop_counter_2; //
        split_check = true;
        loop_counter_2++;
        // check here for the wcet
      }

      else {
        if(secure_loops_2.find(loop_unique_id) == secure_loops_2.end()){
          continue;
        }

        if(secure_loops_2[loop_unique_id].wcet_ns < secure_wcet_ns){
          if(secure_loops_2[loop_unique_id].seq_split != -1){
            secure_loops_2[loop_unique_id].seq_split += 1;
          }

          else {
            secure_loops_2[loop_unique_id].split_factor += 1;
          }

          LoopSplit_2(ltemp, secure_loops_2[loop_unique_id].split_factor, parallel_region_id, -1, loop_counter_2);
          secure_loops_2[loop_unique_id].total_inst = addSeqCallsInLoop(F,ltemp,parallel_region_id,-1,loop_counter_2,secure_loops_2[loop_unique_id].seq_split,MA);
          secure_loops[ltemp] = loop_unique_id;
          loop_details_profiler[parallel_region_id][loop_counter_2] = secure_loops_2[loop_unique_id];
          loop_details_profiler[parallel_region_id][loop_counter_2].parallel_id = parallel_region_id;
          loop_details_profiler[parallel_region_id][loop_counter_2].loop_id = loop_counter_2;
          loop_counter_2++;
          loop_unique_id++;
        }

        else {
          if(secure_loops_2[loop_unique_id].split_factor != 2){
            secure_loops_2[loop_unique_id].split_factor /= 2; // can also reduce by 1 and then keep running multiple phases
          }
          LoopSplit_2(ltemp, secure_loops_2[loop_unique_id].split_factor, parallel_region_id, -1, loop_counter_2);
          int val = secure_loops_2[loop_unique_id].wcet_ns/secure_wcet_ns;
          int split_val = secure_loops_2[loop_unique_id].total_inst/val;
          if(secure_loops_2[loop_unique_id].seq_split == val){
            split_val = split_val/2; // can be -1 or /2 just a proof of concept
          }

          secure_loops_2[loop_unique_id].seq_split = split_val;
          secure_loops_2[loop_unique_id].total_inst = addSeqCallsInLoop(F,ltemp,parallel_region_id,-1,loop_counter_2,secure_loops_2[loop_unique_id].seq_split,MA);
          secure_loops[ltemp] = loop_unique_id;
          loop_details_profiler[parallel_region_id][loop_counter_2] = secure_loops_2[loop_unique_id];
          loop_details_profiler[parallel_region_id][loop_counter_2].parallel_id = parallel_region_id;
          loop_details_profiler[parallel_region_id][loop_counter_2].loop_id = loop_counter_2;
          loop_counter_2++;
          loop_unique_id++;

        }
      }
    }
  }
}

std::string dumptest_2;
raw_string_ostream dumpdata_2(dumptest_2);

void updateWorkId_2(Module &M, Function &F, LLVMContext &CTX, ModuleAnalysisManager &MA){
  int ctr = 1, p_id;
  MDNode* parallel_id = F.getMetadata("parallel_id");
  Value* parallel_id_temp = dyn_cast<ValueAsMetadata>(parallel_id->getOperand(0))->getValue();
  auto* ci = dyn_cast<ConstantInt>(parallel_id_temp);
  p_id = ci->getZExtValue()-1;

  auto &Options = cl::getRegisteredOptions();
  if (!(Options.count("check_file") && Myfile)){
    MDNode* ttex_array = F.getMetadata("ttex_array");
    MDNode* ttex_sub_array = F.getMetadata("ttex_sub_array");

    if(ttex_array && ttex_sub_array){
      errs()<<"ttex array available"<<"\n";
      Value* v = dyn_cast<ValueAsMetadata> (ttex_array->getOperand(0))->getValue();
      Value* v_sub = dyn_cast<ValueAsMetadata> (ttex_sub_array->getOperand(0))->getValue();
      if(v && v_sub){
        errs()<<"value received from ttex array"<<"\n";
        // errs()<<"Retreiving num elements for each outlined 2"<<n<<"\n";
        ConstantDataArray* init = dyn_cast<ConstantDataArray> (v);
        ConstantDataArray* init_sub = dyn_cast<ConstantDataArray> (v_sub);
        if(init && init_sub){
          errs()<<"array value of ttex array received"<<"\n";
          int n = init->getNumElements(); // num elements should be the same
          std::vector<para_details> pd_temp;
          for(unsigned i = 0 ; i < n ; i++){
            para_details pd;
            pd.parallel_id = p_id;
            pd.ref = init->getElementAsInteger(i);
            pd.id = init_sub->getElementAsInteger(i);
            pd.wcet_ns = 0;
            pd.seq_split = -1;
            pd_temp.push_back(pd);
          }

          region_details_profiler[p_id] = pd_temp;
        }
      }
    }
  }

  auto &FM = MA.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  FM.invalidate(F,PreservedAnalyses::none());
  LoopInfo *LI = &FM.getResult<LoopAnalysis>(F);

  int count = 0;

  for (Function::iterator block_iter = F.begin(), block_iter_end = F.end(); block_iter != block_iter_end; ++block_iter) {
    BasicBlock &B = *block_iter;

    for(BasicBlock::iterator instr_iter = B.begin(), instr_iter_end = B.end(); instr_iter != instr_iter_end; ++instr_iter){
      Instruction &I = *instr_iter;
      Loop *temp_loop = LI->getLoopFor(&B);
      if(temp_loop == nullptr) {
        count++;
      }

      if(CallInst* call_inst = dyn_cast<CallInst>(&I)){
        Function* fn = call_inst->getCalledFunction();
        //errs()<<fn->dump()<<"\n";
        call_inst->print(dumpdata_2,false);
        //errs()<<"call instruction is "<<dumptest_2<<"\n";
        if(fn){
            if(fn->getName() == "__kmpc_for_static_init_4"){
                llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
                call_inst->setOperand(9,itr_ci);
                region_details_profiler[p_id][ctr].total_inst = count;
                count = 0;
                ctr++;
            }

            else if(fn->getName() == "__kmpc_single"){
                llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
                call_inst->setOperand(2,itr_ci);
                region_details_profiler[p_id][ctr].total_inst = count;
                count = 0;
                ctr++;
            }

            else {
              count = returnInstCount(fn, MA, p_id, ctr, -1, region_details_profiler[p_id][ctr].seq_split, count);
            }
        }
      }
    }
  }

  splittingLoops(F,p_id,MA);
  /*end of loop split code*/
}

PreservedAnalyses TtexUpdatePassV2::run(Module &M, ModuleAnalysisManager &MA) {

    LLVMContext &CTX = M.getContext();

    int counter = 0;

    auto &Options = cl::getRegisteredOptions();
    if (Options.count("check_file") && Myfile){
      std::ifstream inFile("/home/swastik/dev/ttex/llvm/ttex_implementation/benchmarks/testbench/data_log_to_pass.txt",std::ios::binary);

      if (inFile) {
          // Read the data from the file
          size_t vectorSizeRow;
          inFile.read(reinterpret_cast<char*>(&vectorSizeRow), sizeof(vectorSizeRow));
          loop_details_profiler.resize(vectorSizeRow);
          for (auto& row : loop_details_profiler) {
              size_t vectorSizeColumn;
              inFile.read(reinterpret_cast<char*>(&vectorSizeColumn), sizeof(vectorSizeColumn));
              row.resize(vectorSizeColumn);
              for (auto& cell : row) {
                  inFile.read(reinterpret_cast<char*>(&cell), sizeof(loop_details_pass));
              }
          }

          // remember to handler parallel begin as the first region
          size_t vector2SizeRow;
          region_details_profiler.resize(vector2SizeRow);
          for (auto& row : region_details_profiler) {
              size_t vector2SizeColumn;
              inFile.read(reinterpret_cast<char*>(&vector2SizeColumn), sizeof(vector2SizeColumn));
              row.resize(vector2SizeColumn);
              for (auto& cell : row) {
                  inFile.read(reinterpret_cast<char*>(&cell), sizeof(para_details));
              }
          }
          //inFile.read(reinterpret_cast<char*>(l_data.data()), vectorSize * sizeof(loop_details_pass));
          inFile.close();

          // Common loops in two parallel regions are handled below - one with higher wcet is preferred if less take other one
          for (const auto& item : loop_details_profiler) {
            for(const loop_details_pass& item_2: item) {
              std::cout<<"loop id:" << item_2.loop_id << std::endl;
              std::cout<<"Parallel id:" << item_2.parallel_id << std::endl;
              std::cout<<"split factor:" << item_2.split_factor << std::endl;
              std::cout<<"seq id:" << item_2.seq_split << std::endl;
              std::pair<int,int> temp = std::make_pair(item_2.parallel_id, item_2.loop_id);
              //secure_loops[secure_loops_2[temp]] = item_2; // just to get the updated wcet
              if(secure_loops_2.find(item_2.unique_loop_id) != secure_loops_2.end()){
                if(secure_loops_2[item_2.unique_loop_id].wcet_ns < item_2.wcet_ns){
                  secure_loops_2[item_2.unique_loop_id] = item_2; // store the higher wcet value as that needs to be split  
                }
              }

              else {
                secure_loops_2[item_2.unique_loop_id] = item_2;
              }
            }

            std::cout<<std::endl;
          }
      } else {
          std::cerr << "Error opening the file for reading." << std::endl;
      }
    }

    for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
      Function &F = *func_iter;

      if (!F.isDeclaration()) {
          //errs()<<"Function name is:"<<F.getName()<<"\n";
        if(F.getName().contains(".omp_outlined.") && F.getName() != ".omp_outlined._debug__"){
          counter++;
        }
      }
    }

    if (!(Options.count("check_file") && Myfile)){
      loop_details_profiler = std::vector < std::vector<loop_details_pass> > (counter, std::vector<loop_details_pass>());
      region_details_profiler = std::vector < std::vector<para_details> > (counter, std::vector<para_details>());
      errs()<<"---------------------------- file option not set----------------------\n";
    }

    for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
    
      Function &F = *func_iter;

      if (!F.isDeclaration()) {
        //errs()<<"Function name is:"<<F.getName()<<"\n";
        if(F.getName().contains(".omp_outlined.") && F.getName() != ".omp_outlined._debug__"){
          //errs()<<"omp outlined function called\n";
          //errs()<<"Function name is:"<<F.getName()<<"\n";
          updateWorkId_2(M,F,CTX,MA);
        }
      }
    }

    // function to read the file for loop details  
    // before updating look up table here, update the sequential iteration based on wcet

    /**
     * 
     * process the complete unordered map and check wcet of any is > stored value - value stored should also be in the file
     * profiler reads a file and updated by the pass and updates a file for the pass whereas pass reads both the file
    */

    // std::vector<loop_details_pass> temp_loop_profile;

    // for(const auto& loop_val: secure_loops_2){
    //   loop_details_pass lt = loop_val.second;
    //   temp_loop_profile.push_back(lt);
    //   std::cout<<"loop id:" << lt.loop_id << std::endl;
    //   std::cout<<"Parallel id:" << lt.parallel_id << std::endl;
    //   std::cout<<"split factor:" << lt.split_factor << std::endl;
    //   std::cout<<"seq id:" << lt.seq_split << std::endl;
    // }

    std::ofstream outFile("/home/swastik/dev/ttex/llvm/ttex_implementation/benchmarks/testbench/data_log.txt",std::ios::binary);

    if (outFile) {
        // Write the number of rows
        size_t numRows = loop_details_profiler.size();
        outFile.write(reinterpret_cast<const char*>(&numRows), sizeof(numRows));

        // Write each row's size and data
        for (const auto& row : loop_details_profiler) {
            size_t rowSize = row.size();
            outFile.write(reinterpret_cast<const char*>(&rowSize), sizeof(rowSize));
            for (const loop_details_pass& cell : row) {
                outFile.write(reinterpret_cast<const char*>(&cell), sizeof(loop_details_pass));
            }
        }

        size_t num2Rows = region_details_profiler.size();
        outFile.write(reinterpret_cast<const char*>(&num2Rows), sizeof(num2Rows));

        // Write each row's size and data
        for (const auto& row : region_details_profiler) {
            size_t rowSize = row.size();
            outFile.write(reinterpret_cast<const char*>(&rowSize), sizeof(rowSize));
            for (const para_details& cell : row) {
                outFile.write(reinterpret_cast<const char*>(&cell), sizeof(para_details));
            }
        }

        outFile.close();
    } else { 
        std::cout<< "Error opening the file for writing.\n";
    }

    return PreservedAnalyses::all();
  }

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "TtexUpdate", "v0.1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef PassName, ModulePassManager &MPM, ...) {
          if(PassName == "texupdate"){
            MPM.addPass(TtexUpdatePassV2());
            return true;
          }
          return false;
        }
      );
    }
  };
}