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
#include "llvm-c/lto.h"
#include "llvm/Analysis/LoopInfo.h"
using namespace llvm;

// namespace llvm {
//   void initializeTtexPassPass (PassRegistry&);
// } // end namespace llvm

static cl::opt<bool> input_file("check_file",
  cl::desc("read update from file"),
  cl::init(false));

static cl::opt<bool> input_ttex("set_ttex",
  cl::desc("enable ttex security"),
  cl::init(false));

static cl::opt<int> nthreads("nthreads",
  cl::desc("number of threads for security"),
  cl::init(1));

static cl::opt<unsigned long int> secure_wcet("threshold",
  cl::desc("threshold for security"),
  cl::init(500000)); // 500us = 500000ns

static cl::opt<int> splitval("splitval",
  cl::desc("loopsplit val"),
  cl::init(10000000)); // 500us = 500000ns

bool check = false;

unsigned long int  secure_wcet_ns = secure_wcet; // earlier value was quite high (9000000)
//double secure_wcet_ns = secure_wcet;
int loop_unique_id = 0;
int function_unique_id = 0;

typedef struct loop_details_pass {
  int parallel_id;
  int loop_id;
  int split_factor;
  int unique_loop_id;
  int seq_split;
  long int total_inst;
  unsigned long int wcet_ns;
  //double wcet_us;
  int total_threads;
  int fns;
  int unique_function_ids[500];
} loop_details_pass;

typedef struct para_details {
  int parallel_id;
  int id;
  int ref;
  int seq_split;
  long int total_inst;
  unsigned long int wcet_ns;
  //double wcet_us;
  int total_threads;
  int fns;
  int unique_function_ids[500];
} para_details;

std::unordered_map<Loop*, int> secure_loops;
std::unordered_map<Function*, int >secure_functions;
std::unordered_map<int, loop_details_pass> secure_loops_2;
std::unordered_map<int , std::vector<int> > secure_functions_2; // vector - parallel_id, sub_id, loop_id
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

void AddFunction(llvm::Module* M, int parallel_id, int sub_id, int loop_id, BasicBlock *block, Instruction *Inst, Value* va){
  llvm::LLVMContext &CTX = M->getContext();

  FunctionType *testing = FunctionType::get(
      Type::getVoidTy(CTX),
      {IntegerType::getInt32Ty(CTX), IntegerType::getInt32Ty(CTX), IntegerType::getInt32Ty(CTX), IntegerType::getInt32Ty(CTX)},
      //PointerType::getPointerAddressSpace(),
      /*IsVarArgs=*/false);

  Function *testfunc = M->getFunction("on_ompt_callback_ompt_test");
  if(testfunc == NULL){
    errs() << "------------------------- ompt_test not found in the module symbol table ----------------------\n";
    //exit(0);
  }

  FunctionCallee hookTest = M->getOrInsertFunction("on_ompt_callback_ompt_test", testing);
  
  std::vector<Value*> args;
  ConstantInt *arg1 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),parallel_id, false);
  ConstantInt *arg2 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),sub_id, false);
  ConstantInt *arg3 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),loop_id, false);
  args.push_back(arg1);
  args.push_back(arg2);
  args.push_back(arg3);
  if(va == nullptr) {
    ConstantInt *arg4 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),-1, false);
    args.push_back(arg4);
  }

  else {
    args.push_back(va);
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
}

bool LoopSplit_2(Loop *L, unsigned count, int parallel_id, int sub_id, int loop_id, ModuleAnalysisManager &MA){

  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *LatchBlock = L->getLoopLatch();

  llvm::Function* Func = Header->getParent();
  llvm::Module *M = Func->getParent();

  std::vector<BasicBlock *> OriginalLoopBlocks = L->getBlocks();

  bool no_latch = false;
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

  errs()<<"checking for errors\n";

  // auto &FM = MA.getResult<FunctionAnalysisManagerModuleProxy>(*M).getManager();
  // ScalarEvolution *SE = FM.getCachedResult<ScalarEvolutionAnalysis>(*Func);

  errs()<<"error evaluating scalar evolution\n";

  // unsigned test = SE->getSmallConstantMaxTripCount(L);

  errs()<<"\n\n\n\n---------------------------------------Loop is -------------------------------"<<Func->getName()<<" "<<parallel_id<<" "<<loop_id<<" "<<Header->getName()<<"\n";

  Loop::LocRange t = L->getLocRange();

  
    DebugLoc StartLoc = t.getStart();
    DebugLoc EndLoc = t.getEnd();


    std::string test_str;
    raw_string_ostream stream(test_str);
    StartLoc->print(stream);

    errs()<< StartLoc.getLine() << " to " << EndLoc.getLine() << " in file \n";
    errs()<<test_str;

  errs()<<"\n\n\n\n";
  

  //attr_set.addAttribute(CTX, AttributeSet::FunctionIndex, Attribute::NoInline);
  //fn_test_2->addAttributes(0, AttributeSet::get(c, AttributeSet::FunctionIndex, attr));
  //security_2.CreateBr(Header);
  Value* ctr_val = security.CreateNSWAdd(counter_val, llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),1, false), "");  
  Value* temp = security.CreateSRem(ctr_val, llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),count, false));
  compare_to_zero = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),0,false);
  Value* compare = security.CreateICmpEQ(temp,compare_to_zero);

  PHINode* p_val = PHINode::Create(llvm::Type::getInt32Ty(CTX), 1, "phi",Secure_2);
  AddFunction(M,parallel_id, sub_id,loop_id,Secure_2,nullptr,p_val);
  llvm::BranchInst::Create(Header,Secure_2);

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
  p_val->addIncoming(ctr_val,Secure_1);

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

std::vector<int> temp_unique_fns;

int returnInstCount(Function *F, ModuleAnalysisManager &MA, int parallel_region_id, int sub_id, int loop_id, int split_id, int count, bool temp_count){
  errs()<<"Function name inside returnInstCount is ----"<<F->getName()<<"\n";

  if(secure_functions_2.find(secure_functions[F]) != secure_functions_2.end()){
    
    errs()<<"secure function found\n";
    
    std::vector<int> test = secure_functions_2[secure_functions[F]];
    if(test[0] != parallel_region_id){
      return count;
    }

    else if(sub_id == -1 && test[2] != loop_id){ // this function might be processed by other region or loop not sure
      //loop_details_profiler[parallel_region_id][loop_id].unique_function_ids.push_back(secure_functions[F]); // this will give seg fault as loop_details_profiler is not initialized yet
      
      // above should be called even if not processing the function as to know this function is in another region
      return count;
    }

    //else if(loop_id == -1 && (test[1] == sub_id || test[2] == sub_id)){
    else if(loop_id == -1 && test[1] != sub_id){  
      return count;
    }
  }

  else {
    errs()<<"new function found\n";
    std::vector<int> test = {parallel_region_id, sub_id,loop_id};
    secure_functions_2[secure_functions[F]] = test;
    errs()<<"Found the secure function id\n";
    if(sub_id == -1){
      errs()<<"loop details profiler search\n";
      if(loop_details_profiler[parallel_region_id].size() > loop_id){
        errs()<<"found the loop details profiler\n";
        loop_details_profiler[parallel_region_id][loop_id].fns += 1;
        int k = loop_details_profiler[parallel_region_id][loop_id].fns;
        loop_details_profiler[parallel_region_id][loop_id].unique_function_ids[k] = secure_functions[F]; // this will give seg fault as loop_details_profiler is not initialized yet
      }

      else {
        errs()<<"profiler not found\n";
        temp_unique_fns.push_back(secure_functions[F]);
      }
    }

    else {
      errs()<<"region details profiler\n";
      region_details_profiler[parallel_region_id][loop_id].fns += 1;
      int k = region_details_profiler[parallel_region_id][loop_id].fns;
      region_details_profiler[parallel_region_id][sub_id].unique_function_ids[k] = secure_functions[F];
    }
  }

  errs()<<"evaluating loop info next\n";

  llvm::Module *M = F->getParent();
  llvm::LLVMContext &CTX = M->getContext();
  auto &FM = MA.getResult<FunctionAnalysisManagerModuleProxy>(*M).getManager();
  //FM.invalidate(*F,PreservedAnalyses::none());
  LoopInfo *LI = &FM.getResult<LoopAnalysis>(*F);

  errs()<<"evaluated loop info\n";

    for (BasicBlock &BB : *F) {

      // break if basic block is a part of a loop in that function
      Loop *temp_loop = LI->getLoopFor(&BB); // will give null for inner loops?
      if(temp_loop) {
        continue; // not handling other loops (Will be handled on their call)
      }

      for (Instruction &I : BB) {

        if(temp_count){
          if(PHINode *Pi = dyn_cast<PHINode>(&I)){

          }

          else {
            AddFunction(M,parallel_region_id,-1,loop_id,nullptr,&I,nullptr);
            temp_count = false;
            split_id = count+split_id;
          }
        }

        if(count == split_id) {

          if(PHINode *Pi = dyn_cast<PHINode>(&I)){
            std::cout<<"######################## Instruction is a phi node #######################"<<std::endl;
            // will need to put it right after the phi node for correct results
            temp_count = true;
          }

          else {
            AddFunction(M,parallel_region_id,sub_id,loop_id,nullptr,&I,nullptr); // sub id in add function is -1 to actual id
            split_id = count+split_id;
            temp_count = false;
          }
        } // check this out

        if (CallInst *CI = dyn_cast<CallInst>(&I)) {
          Function *CalledFunc = CI->getCalledFunction();
          if(secure_functions.find(CalledFunc) == secure_functions.end()){
            if (CalledFunc && !CalledFunc->isDeclaration()) {
              if(!CalledFunc->getName().contains(".omp_outlined.")){
                secure_functions[CalledFunc] = function_unique_id;
                function_unique_id+=1;
                if(CalledFunc->getName() != F->getName()){
                  count = returnInstCount(CalledFunc, MA, parallel_region_id, sub_id, loop_id, split_id, count, temp_count);
                }
              }
            }
          }
        }

        else if (InvokeInst *CI= dyn_cast<InvokeInst>(&I)){
          Function *CalledFunc = CI->getCalledFunction();
          if(secure_functions.find(CalledFunc) == secure_functions.end()){
            if (CalledFunc && !CalledFunc->isDeclaration()) {
              if(!CalledFunc->getName().contains(".omp_outlined.")){
                secure_functions[CalledFunc] = function_unique_id;
                function_unique_id+=1;
                if(CalledFunc->getName() != F->getName()){
                  count = returnInstCount(CalledFunc, MA, parallel_region_id, sub_id, loop_id, split_id, count, temp_count);
                }
              }
            }
          }
        }

        count += 1;
      }
    }

    errs()<<"returning count done with evalauting count for: "<<F->getName()<<"\n"; 

  return count;
}

int addSeqCallsInLoop(Function &F, Loop *L, int parallel_region_id, int sub_id, int loop_id, int split_id, ModuleAnalysisManager &MA, bool temp_count){
  int count = 0;
  std::vector<BasicBlock *> OriginalLoopBlocks = L->getBlocks();

  llvm::Module *M = F.getParent();
  llvm::LLVMContext &CTX = M->getContext();
  auto &FM = MA.getResult<FunctionAnalysisManagerModuleProxy>(*M).getManager();
  //FM.invalidate(F,PreservedAnalyses::none());
  LoopInfo *LI = &FM.getResult<LoopAnalysis>(F);

  for(int i = 0 ; i < OriginalLoopBlocks.size() ; i++){
    BasicBlock *LoopBlock = OriginalLoopBlocks[i];

    Loop *temp_loop = LI->getLoopFor(LoopBlock);
    if(temp_loop && temp_loop != L) {
      continue;
    }

    for(llvm::BasicBlock::iterator I = LoopBlock->begin(), Iend = LoopBlock->end(); I != Iend ; ++I){
      Instruction *Inst = &*I;
      //Instruction *Inst_temp = &*(--Iend);
      
      if(temp_count){
        if(PHINode *Pi = dyn_cast<PHINode>(Inst)){

        }

        else {
          AddFunction(M,parallel_region_id,-1,loop_id,nullptr,Inst,nullptr);
          temp_count = false;
          split_id = count + split_id;
        }
      }

      if(count == split_id) {
          if(PHINode *Pi = dyn_cast<PHINode>(Inst)){
            std::cout<<"######################## Instruction is a phi node #######################"<<std::endl;
            temp_count = true;
          }

          else {
            if(Inst == &LoopBlock->back()){
              std::cout<<"######################## just checking for last instruction ######################"<<std::endl;
            }
            AddFunction(M,parallel_region_id,-1,loop_id,nullptr,Inst,nullptr);
            split_id = count + split_id; // or could have done count%split_id == 0
            temp_count = false;
          }
      }
      
      if (CallInst *CI = dyn_cast<CallInst>(Inst)) {
        // It's a call instruction
        Function *CalledFunc = CI->getCalledFunction();
        if(secure_functions.find(CalledFunc) == secure_functions.end()){
          if (CalledFunc && !CalledFunc->isDeclaration()) {
            if(!CalledFunc->getName().contains(".omp_outlined.")){
              secure_functions[CalledFunc] = function_unique_id;
              function_unique_id+=1;
              if(CalledFunc->getName() != F.getName()){ // eliminate recursive calls
                count = returnInstCount(CalledFunc, MA, parallel_region_id, sub_id, loop_id, split_id, count, temp_count);
              }
            }
          }
        }
      }

      else if (InvokeInst *CI= dyn_cast<InvokeInst>(Inst)){
        // It's a call instruction
        Function *CalledFunc = CI->getCalledFunction();
        if(secure_functions.find(CalledFunc) == secure_functions.end()){
          if (CalledFunc && !CalledFunc->isDeclaration()) {
            if(!CalledFunc->getName().contains(".omp_outlined.")){
              secure_functions[CalledFunc] = function_unique_id;
              function_unique_id+=1;
              if(CalledFunc->getName() != F.getName()){ // eliminate recursive calls - not needed now
                count = returnInstCount(CalledFunc, MA, parallel_region_id, sub_id, loop_id, split_id, count, temp_count);
              }
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
  
  errs() <<"Checking for errors in generateAllLoops_2\n";

  if(!L){
    return allLoops_2;
  } // without optimization some random null loops are generated

  if(L->isInvalid()){
    return allLoops_2;
  }
  
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
  //FM.invalidate(F,PreservedAnalyses::none());
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

  errs()<<"generate all loops works\n";

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
        if (CallInst *CI = dyn_cast<CallInst>(&I)) {
            // It's a call instruction
            Function *CalledFunc = CI->getCalledFunction();
            if (CalledFunc && !CalledFunc->isDeclaration()) {
              errs()<<"Function name for loop split is:"<<CalledFunc->getName()<<"\n";
              errs()<<"Function is not a declaration\n";
              if(!CalledFunc->getName().contains(".omp_outlined.") || CalledFunc->getName() != "on_ompt_callback_test"){
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
            if (CalledFunc && !CalledFunc->isDeclaration()) {
              errs()<<"Function name for loop split is:"<<CalledFunc->getName()<<"\n";
              errs()<<"Function is not a declaration\n";
              if(!CalledFunc->getName().contains(".omp_outlined.") || CalledFunc->getName() != "on_ompt_callback_test"){
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

  return allLoops_2; // return
}

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

    else {
      continue;
    }
    
    if(ExitingBlock){
      errs()<<"exiting found"<<"\n";
      ExitingBlock->printAsOperand(stream3,false);
      errs()<<eg_label<<"\n";
    }

    else {
      continue;
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
    if (!(Options.count("check_file") && input_file)){
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
        split_check = LoopSplit_2(ltemp, splitval, parallel_region_id, -1, loop_counter_2,MA);
        if(split_check){
          loop_details_pass lt;
          lt.parallel_id = parallel_region_id;
          lt.loop_id = loop_counter_2;
          lt.total_inst = addSeqCallsInLoop(F,ltemp,parallel_region_id,-1,loop_counter_2,-1,MA,false); // now add
          lt.seq_split = -1;
          lt.split_factor = splitval; // setting default split factor to 10
          lt.unique_loop_id = loop_unique_id;
          lt.wcet_ns = 0;
          //lt.wcet_us = 0;
          lt.total_threads = nthreads;
          lt.fns = temp_unique_fns.size();
          for(int k = 0 ; k < temp_unique_fns.size() ; k++){
            lt.unique_function_ids[k] = temp_unique_fns[k];
          }
          temp_unique_fns.clear();
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

    else {  
      
      for(int z = 0 ; z < loop_details_profiler[parallel_region_id].size() ; z++){
        std::cout<<"sub id id: " << loop_details_profiler[parallel_region_id][z].loop_id <<std::endl;
        std::cout<<"Parallel id: " << loop_details_profiler[parallel_region_id][z].parallel_id <<std::endl;
        std::cout<<"split factor: " << loop_details_profiler[parallel_region_id][z].split_factor << std::endl;
        std::cout<<"total instructions: " <<loop_details_profiler[parallel_region_id][z].total_inst<<std::endl;
        std::cout<<"seq split: "<<loop_details_profiler[parallel_region_id][z].seq_split<<std::endl;
      }

      // this will change on the basis of new split analysis given wcet
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

        if(secure_loops_2[loop_unique_id].wcet_ns < secure_wcet_ns){ // secure wcet is the threshold and not average
        //if(secure_loops_2[loop_unique_id].wcet_us < secure_wcet_ns){  
          // if(secure_loops_2[loop_unique_id].seq_split != -1){
          //   secure_loops_2[loop_unique_id].seq_split += 1;
          // }

          // else {
          //   secure_loops_2[loop_unique_id].split_factor += 1;
          // }

          LoopSplit_2(ltemp, secure_loops_2[loop_unique_id].split_factor, parallel_region_id, -1, loop_counter_2,MA);
          secure_loops_2[loop_unique_id].total_inst = addSeqCallsInLoop(F,ltemp,parallel_region_id,-1,loop_counter_2,secure_loops_2[loop_unique_id].seq_split,MA,false);
          secure_loops[ltemp] = loop_unique_id;
          loop_details_profiler[parallel_region_id][loop_counter_2] = secure_loops_2[loop_unique_id];
          loop_details_profiler[parallel_region_id][loop_counter_2].parallel_id = parallel_region_id;
          loop_details_profiler[parallel_region_id][loop_counter_2].loop_id = loop_counter_2;
          loop_counter_2++;
          loop_unique_id++;
        }

        else {
          //secure_loops_2[loop_unique_id].split_factor /= 2; // can also reduce by 1 (=-1) and then keep running multiple phases

          if(secure_loops_2[loop_unique_id].split_factor / 2 == 0){
            if(secure_loops_2[loop_unique_id].seq_split == 1){
              std::cout<<"Threshold value too low for security"<<std::endl;
              //exit(0);
            }

            else if(secure_loops_2[loop_unique_id].seq_split == -1){
              long int val = (secure_loops_2[loop_unique_id].wcet_ns/secure_wcet_ns)+1;
              // errs() << "Instruction count is" << secure_loops_2[loop_unique_id].total_inst <<"---------------\n";
              // errs() << "WCET is "<<secure_loops_2[loop_unique_id].wcet_ns/secure_wcet_ns<<" \n";
              // errs() << "evaluated val is" << val << "----------- parallel id values is: "<< parallel_region_id <<"\n";
              int split_val = secure_loops_2[loop_unique_id].total_inst/val;
              if(secure_loops_2[loop_unique_id].seq_split == split_val){
                split_val = split_val/2; // can be -1 or /2 just a proof of concept
              }
              // secure_loops_2[loop_unique_id].seq_split = split_val;

              //Updating the above logic a bit
              secure_loops_2[loop_unique_id].seq_split = secure_loops_2[loop_unique_id].total_inst/2;
            }

            else {
              secure_loops_2[loop_unique_id].seq_split /= 2;
              if(secure_loops_2[loop_unique_id].seq_split == 0){
                secure_loops_2[loop_unique_id].seq_split = 1;
              }
            }

            secure_loops_2[loop_unique_id].split_factor = 1; // split_factor cannot be 0
          }

          else {
            secure_loops_2[loop_unique_id].split_factor /= 2; // first work with reducing the split factor then the split of an iteration
            // if(secure_loops_2[loop_unique_id].seq_split == 0){
            //   secure_loops_2[loop_unique_id].seq_split = 1;
            // }
          }

          LoopSplit_2(ltemp, secure_loops_2[loop_unique_id].split_factor, parallel_region_id, -1, loop_counter_2,MA);
          secure_loops_2[loop_unique_id].total_inst = addSeqCallsInLoop(F,ltemp,parallel_region_id,-1,loop_counter_2,secure_loops_2[loop_unique_id].seq_split,MA,false);
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

  std::cout<<"----------------- update work id--------------------"<<std::endl;

  auto &Options = cl::getRegisteredOptions();
  if (!(Options.count("check_file") && input_file)){
    MDNode* ttex_array = F.getMetadata("ttex_array");
    MDNode* ttex_sub_array = F.getMetadata("ttex_sub_array");

    if(ttex_array && ttex_sub_array){
      errs() << "function name is:" << F.getName() <<"\n";
      errs() << "ttex array available" << "\n";
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

          //push back for parallel begin first
          para_details temp_pd;
          temp_pd.parallel_id = p_id;
          temp_pd.ref = -1000;
          temp_pd.id = 0;
          temp_pd.wcet_ns = 0;
          //temp_pd.wcet_us = 0;
          temp_pd.seq_split = -1;
          temp_pd.total_threads = nthreads;
          temp_pd.total_inst = 0;
          pd_temp.push_back(temp_pd);

          for(int i = 0 ; i < n ; i++){
            errs()<<"sub value is "<<init_sub->getElementAsInteger(i)<<"\n";
            para_details pd;
            pd.parallel_id = p_id;
            pd.ref = init->getElementAsInteger(i);
            //pd.id = init_sub->getElementAsInteger(i);
            pd.id = i+1;
            pd.wcet_ns = 0;
            //pd.wcet_us = 0;
            pd.seq_split = -1;
            pd.total_threads = nthreads;
            pd.total_inst = 0;
            pd_temp.push_back(pd);
          }

          errs()<<"region details profiler search\n";
          region_details_profiler[p_id] = pd_temp;
          errs()<<"region details profiler found\n";
        }
      }
    }
  }

  else {

    // file was read so lets evaluate seq split factor
    for (int i = 0 ; i < region_details_profiler[p_id].size() ; i++) {

      // std::cout<<"sub id:" << region_details_profiler[p_id][i].id <<"\n";
      // std::cout<<"Parallel id:" << region_details_profiler[p_id][i].parallel_id <<"\n";
      // std::cout<<"split factor:" << region_details_profiler[p_id][i].seq_split << "\n";
      // std::cout<<"total instructions:" <<region_details_profiler[p_id][i].total_inst<<"\n";
      // std::cout<<"region id is:" <<region_details_profiler[p_id][i].ref<<"\n";
      // std::cout<<"wcet time value is: " << region_details_profiler[p_id][i].wcet_ns;
      // std::cout<<std::endl;

      if(region_details_profiler[p_id][i].wcet_ns > secure_wcet_ns){
      //if(region_details_profiler[p_id][i].wcet_us > secure_wcet_ns){

        if(region_details_profiler[p_id][i].seq_split == 1){
          std::cout<<"Threshold value too low for security"<<std::endl;
          //exit(0);
        }

        else if(region_details_profiler[p_id][i].seq_split != -1){
          region_details_profiler[p_id][i].seq_split /= 2;
          if(region_details_profiler[p_id][i].seq_split == 0){
            region_details_profiler[p_id][i].seq_split == 1;
          }
        }

        else {
          int val = (region_details_profiler[p_id][i].wcet_ns/secure_wcet_ns)+1;
          int split_val = region_details_profiler[p_id][i].total_inst/val;
          if(region_details_profiler[p_id][i].seq_split == split_val){
            split_val = split_val/2; // can be -1 or /2 just a proof of concept
          }

          //region_details_profiler[p_id][i].seq_split = split_val;
          region_details_profiler[p_id][i].seq_split = region_details_profiler[p_id][i].total_inst/2;
          if(region_details_profiler[p_id][i].seq_split == 0){
            region_details_profiler[p_id][i].seq_split == 1;
          }
        }
      }
    }
  }

  std::cout<<"------------------------end of region details evaluation for parallel id "<< p_id <<"--------------------------------"<<std::endl;

  auto &FM = MA.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  //FM.invalidate(F,PreservedAnalyses::none());
  LoopInfo *LI = &FM.getResult<LoopAnalysis>(F);

  int count = 0;
  bool temp_count = false;
  int split_id = region_details_profiler[p_id][ctr-1].seq_split;

  for (Function::iterator block_iter = F.begin(), block_iter_end = F.end(); block_iter != block_iter_end; ++block_iter) {
    BasicBlock &B = *block_iter;

    // if(p_id == 5){
    //   errs()<<"^^^^^^^^^^^^^ instruction count till now is : "<<count<<"\n";
    // }

    Loop *temp_loop = LI->getLoopFor(&B);
    if(temp_loop == nullptr) {
      //std::cout<<"basic block is part of the loop for parallel id "<<B.getName().str()<<" "<<p_id<<std::endl;
      //continue; // if instruction belongs to a basic block which is the part of a loop then don't process it
    
      for(BasicBlock::iterator instr_iter = B.begin(), instr_iter_end = B.end(); instr_iter != instr_iter_end; ++instr_iter){
        Instruction &I = *instr_iter;

        if(temp_count){
          if(PHINode *Pi = dyn_cast<PHINode>(&I)){

          }

          else {
            AddFunction(&M,p_id,ctr-1,-1,nullptr,&I,nullptr);
            temp_count = false;
            split_id = count+split_id;
          }
        }

        if(count == split_id) {

          if(PHINode *Pi = dyn_cast<PHINode>(&I)){
            std::cout<<"######################## Instruction is a phi node #######################"<<std::endl;
            // will need to put it right after the phi node for correct results
            temp_count = true;
          }

          else {
            AddFunction(&M,p_id,ctr-1,-1,nullptr,&I,nullptr); // sub id in add function is -1 to actual id
            split_id = count+split_id;
            temp_count = false;
          }
        } // check this out

        if(CallInst* call_inst = dyn_cast<CallInst>(&I)){
          Function* fn = call_inst->getCalledFunction();
          //errs()<<fn->dump()<<"\n";
          call_inst->print(dumpdata_2,false);
          //errs()<<"call instruction is "<<dumptest_2<<"\n";
          if(fn){
            if(fn->getName() == "__kmpc_for_static_init_4"){
                llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
                call_inst->setOperand(9,itr_ci);
                region_details_profiler[p_id][ctr-1].total_inst = count;
                // errs()<<"total instruction for parallel region "<<p_id<<" and sub region "<<ctr-1<<"\n";
                // errs()<<"set region details profiler with counter in for with instruction count till now is "<<region_details_profiler[p_id][ctr-1].total_inst<<" "<<count<<"\n";
                // errs()<<"for region details profiler is set\n";
                count = 0; // let's directly operate on total inst 
                ctr++;
                split_id = region_details_profiler[p_id][ctr-1].seq_split;
                temp_count = false;
            }

            else if(fn->getName() == "__kmpc_single"){
                llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
                call_inst->setOperand(2,itr_ci);
                errs()<<"set region details profiler with counter in single\n";
                region_details_profiler[p_id][ctr-1].total_inst = count;
                errs()<<"singles region detail profiler is set\n"; 
                count = 0;
                ctr++;
                split_id = region_details_profiler[p_id][ctr-1].seq_split;
                temp_count = false;
            }

            else {
              if (!fn->isDeclaration()) {
                if(!fn->getName().contains(".omp_outlined.")){
                  if(secure_functions.find(fn) == secure_functions.end()){
                    secure_functions[fn] = function_unique_id;
                    function_unique_id+=1;
                    count = returnInstCount(fn, MA, p_id, ctr-1, -1, split_id, count, temp_count);
                    //errs()<<"Instruction count recorded till now is "<<count<<"\n";
                    // region_details_profiler[p_id][ctr].total_inst = count;
                    // count = 0;
                  }
                }
              }  
            }
          }
        }

        else if (InvokeInst *CI= dyn_cast<InvokeInst>(&I)){
          Function *fn = CI->getCalledFunction();
          if (fn && !fn->isDeclaration()) {
            if(!fn->getName().contains(".omp_outlined.")){
              if(secure_functions.find(fn) == secure_functions.end()){
                secure_functions[fn] = function_unique_id;
                function_unique_id+=1;
                count = returnInstCount(fn, MA, p_id, ctr-1, -1, split_id, count, temp_count);
              }
            }
          }
        }

        else {
          count++; // if neither a call on invoke instruction instruction is to be added
        }
      }
    }
  }

  if(count != 0) {
    region_details_profiler[p_id][ctr-1].total_inst = count; // this is needed as the last part total instruction (last sub id) is not being set (everything before kmpc static is considered as id 0)
  }

  /*
    Sequential codes protected first as Functions are only protected once and if a loop calls it again it will not be protected
    other way round it will be difficult to determine as sub_id passed would be -1

    also sequential code will have highest wcet so better to handle sequential code handling it
    even if loop executing is sequential it executes with same WCET
    */
  
  std::cout<<"-----------------------------------------------------------------------------------------------"<<std::endl; 
  splittingLoops(F,p_id,MA); // some issue in the loop - debug this 
  std::cout<<"-----------------------------------------------------------------------------------------------"<<std::endl;
  // errs()<<"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n\n"; 
  // errs()<<"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n\n";

  // for(int k = 0 ; k < region_details_profiler.size() ; k++) {
  //   for(int h = 0 ; h < region_details_profiler[k].size(); h++) {
  //     std::cout<<"sub id:" << region_details_profiler[k][h].id <<"\n";
  //     std::cout<<"Parallel id:" << region_details_profiler[k][h].parallel_id <<"\n";
  //     std::cout<<"split factor:" << region_details_profiler[k][h].seq_split << "\n";
  //     std::cout<<"total instructions:" <<region_details_profiler[k][h].total_inst<<"\n";
  //     std::cout<<"region id is:" <<region_details_profiler[k][h].ref<<"\n";
  //   }
  //   std::cout<<std::endl;
  //   std::cout<<std::endl;
  // }

  //  errs()<<"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n\n";

  // std::cout<<std::endl;
  // std::cout<<std::endl;

  // std::cout<<"------------------- loop details ------------------------------"<<std::endl;
  // std::cout<<std::endl;

  // for (const auto& item : loop_details_profiler) {
  //   for(const loop_details_pass& item_2: item) {
  //     std::cout<<"sub id id:" << item_2.loop_id <<std::endl;
  //     std::cout<<"Parallel id:" << item_2.parallel_id <<std::endl;
  //     std::cout<<"split factor:" << item_2.seq_split << std::endl;
  //     std::cout<<"total instructions:" <<item_2.total_inst<<std::endl;
  //   }

  //   std::cout<<std::endl;
  // }

  // std::cout<<std::endl;
  // std::cout<<std::endl;


  // std::cout<<"------------------- region details ----------------------------"<<std::endl;
  // std::cout<<std::endl;

  // for (const auto& item : region_details_profiler) {
  //   for(const para_details& item_2: item) {
  //     std::cout<<"sub id id:" << item_2.id <<std::endl;
  //     std::cout<<"Parallel id:" << item_2.parallel_id <<std::endl;
  //     std::cout<<"split factor:" << item_2.seq_split << std::endl;
  //     std::cout<<"total instructions:" <<item_2.total_inst<<std::endl;
  //     std::cout<<"region id is:" <<item_2.ref<<std::endl;
  //   }

  //   std::cout<<std::endl;
  // }

  std::cout<<std::endl;
  std::cout<<std::endl;

  std::cout<<"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<std::endl;
  std::cout<<"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<std::endl;
  std::cout<<std::endl;
  /*end of loop split code*/
}

PreservedAnalyses TtexUpdatePassV2::run(Module &M, ModuleAnalysisManager &MA) {

    LLVMContext &CTX = M.getContext();

    int p_counter = 0;

    std::cout<<"+++++++++++++++++++++++++++++++++++++++ secure time is: ++++++++"<<secure_wcet<<std::endl;
    secure_wcet_ns = secure_wcet;

    auto &Options = cl::getRegisteredOptions();
    errs()<<"Options value is: "<<Options.count("set_ttex")<<" \n";
    if ((Options.count("set_ttex") && input_ttex)){
      return PreservedAnalyses::all();
    }

    // auto &Options = cl::getRegisteredOptions();
    if (Options.count("check_file") && input_file){
      std::ifstream inFile("/home/swastik/dev/ttex/llvm/ttex_implementation/benchmarks/testbench/data_log_to_pass.txt",std::ios::binary);

      errs()<<" Reading from a file llvm\n";

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

          errs()<<"done reading loops\n";

          for (const auto& item : loop_details_profiler) {
            for(const loop_details_pass& item_2: item) {
              errs()<<"loop id:" << item_2.loop_id <<"\n";
              errs()<<"Parallel id:" << item_2.parallel_id <<"\n";
              errs()<<"split factor:" << item_2.split_factor << "\n";
              errs()<<"seq id:" << item_2.seq_split <<"\n";
              errs()<<"wcet is:" << item_2.wcet_ns << "\n";
              errs()<<"total instructions:" <<item_2.total_inst<<"\n";
            }

            std::cout<<std::endl;
          }

          errs()<<"Now reading region details...\n";

          // remember to handler parallel begin as the first region
          size_t vector2SizeRow;
          inFile.read(reinterpret_cast<char*>(&vector2SizeRow), sizeof(vector2SizeRow));
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

          errs()<<"Reading parallel regions done\n";

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

              for(int k = 0 ; k < (item_2.fns) ; k++) {
                if(secure_functions_2.find(item_2.unique_function_ids[k]) != secure_functions_2.end()) {
                  std::vector<int> test = secure_functions_2[item_2.unique_function_ids[k]];
                  if(test[1] == -1){
                    // another loop
                    if(loop_details_profiler[test[0]][test[2]].wcet_ns < item_2.wcet_ns){
                      std::vector<int> test2 = {item_2.parallel_id, -1, item_2.loop_id};
                      secure_functions_2[item_2.unique_function_ids[k]] = test2;
                    }
                  }

                  else if(test[2] == -1){
                    // another loop
                    if(region_details_profiler[test[0]][test[1]].wcet_ns < item_2.wcet_ns){
                      std::vector<int> test2 = {item_2.parallel_id, -1, item_2.loop_id};
                      secure_functions_2[item_2.unique_function_ids[k]] = test2;
                    }
                  }
                }

                else {
                  std::vector<int> test2 = {item_2.parallel_id, -1, item_2.loop_id};
                  secure_functions_2[item_2.unique_function_ids[k]] = test2;
                  //secure_functions_2[item_2.unique_function_ids[k]] = test2;
                }  
              }  
            }

            std::cout<<std::endl;
          }

          for (const auto& item : region_details_profiler) {
            for(const para_details& item_2: item) {
              for(int k = 0 ; k < (item_2.fns) ; k++) {
                if(secure_functions_2.find(item_2.unique_function_ids[k]) != secure_functions_2.end()) {
                  std::vector<int> test = secure_functions_2[item_2.unique_function_ids[k]];
                  if(test[1] == -1){
                    // another loop
                    if(loop_details_profiler[test[0]][test[2]].wcet_ns < item_2.wcet_ns){
                      std::vector<int> test2 = {item_2.parallel_id, item_2.id, -1};
                      secure_functions_2[item_2.unique_function_ids[k]] = test2;
                    }
                  }

                  else if(test[2] == -1){
                    // another loop
                    if(region_details_profiler[test[0]][test[1]].wcet_ns < item_2.wcet_ns){
                      std::vector<int> test2 = {item_2.parallel_id, item_2.id, -1};
                      secure_functions_2[item_2.unique_function_ids[k]] = test2;
                    }
                  }
                }

                else { // make sure the unique functions id is inserted to other regions else unique functions id will always be different
                  std::vector<int> test2 = {item_2.parallel_id, item_2.id, -1};
                  secure_functions_2[item_2.unique_function_ids[k]] = test2;
                  //secure_functions_2[item_2.unique_function_ids[k]] = test2;
                }  
              }
            }
          }

      } else {
          std::cerr << "Error opening the file for reading." << std::endl;
      }
    }

    for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
      Function &F = *func_iter;

      if (!F.isDeclaration()) {
          //errs()<<"Function name is:"<<F.getName()<<"\n";
        if(F.getName().contains(".omp_outlined.") && !F.getName().contains("debug")){
          p_counter++;
        }
      }
    }

    if (!(Options.count("check_file") && input_file)){
      loop_details_profiler = std::vector < std::vector<loop_details_pass> > (p_counter, std::vector<loop_details_pass>());
      region_details_profiler = std::vector < std::vector<para_details> > (p_counter, std::vector<para_details>());
      errs()<<"---------------------------- file option not set----------------------\n";
    }

    for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
    
      Function &F = *func_iter;

      if (!F.isDeclaration()) {
        //errs()<<"Function name is:"<<F.getName()<<"\n";
        if(F.getName().contains(".omp_outlined.") && !F.getName().contains("debug")){ //F.getName() != ".omp_outlined._debug__"){
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


    // std::cout<<"============================================= printing file details ================================="<<std::endl;

    // for (const auto& item : region_details_profiler) {
    //   for(const para_details& item_2: item) {
    //     std::cout<<"sub id:" << item_2.id <<"\n";
    //     std::cout<<"Parallel id:" << item_2.parallel_id <<"\n";
    //     std::cout<<"split factor:" << item_2.seq_split << "\n";
    //     std::cout<<"total instructions:" <<item_2.total_inst<<"\n";
    //     std::cout<<"region id is:" <<item_2.ref<<"\n";
    //   }
    // }

    // std::cout<< "=========================details printed====================" <<std::endl;

    // for(int k = 0 ; k < region_details_profiler.size() ; k++) {
    //   for(int h = 0 ; h < region_details_profiler[k].size(); h++) {
    //     std::cout<<"sub id:" << region_details_profiler[k][h].id <<"\n";
    //     std::cout<<"Parallel id:" << region_details_profiler[k][h].parallel_id <<"\n";
    //     std::cout<<"split factor:" << region_details_profiler[k][h].seq_split << "\n";
    //     std::cout<<"total instructions:" <<region_details_profiler[k][h].total_inst<<"\n";
    //     std::cout<<"region id is:" <<region_details_profiler[k][h].ref<<"\n";
    //   }  
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

    std::cout<<"--------=========== end of pass ==================----------------"<<std::endl;

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