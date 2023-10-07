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

std::vector< std::vector< std::pair<int,int> > > astdata_2; //storing the sub region info -- but need to fix the id's to correct location
std::vector<int> sizes_2;
std::vector< std::vector<int> > loop_split_2;

int loop_counter_2 = 0;

bool maxvuln_set_2 = true;

#define omp_for_ref -1
#define omp_sections_ref 0
#define omp_single_ref -2

bool LoopSplit_2(Loop *L, unsigned count, int parallel_id, int sub_id, int counter){

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

  FunctionType *testing = FunctionType::get(
      Type::getVoidTy(CTX),
      {IntegerType::getInt32Ty(CTX), IntegerType::getInt32Ty(CTX), IntegerType::getInt32Ty(CTX), IntegerType::getInt32Ty(CTX)},
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
  ConstantInt *arg3 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),counter, false);
  ConstantInt *arg4 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),-1, false);
  args.push_back(arg1);
  args.push_back(arg2);
  args.push_back(arg3);
  args.push_back(arg4);
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
  CallInst *callinst = llvm::CallInst::Create(hookTest,args,"",Secure_2);
  Function *fn_test_2 = callinst->getCalledFunction();
  std::vector<Attribute> attr_list;
  AttributeSet attr_set = AttributeSet::get(CTX, attr_list);
  fn_test_2->addFnAttr(Attribute::NoInline);
  fn_test_2->addFnAttr(Attribute::NoUnwind);
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

void setLookUpTable_2(Module &M, Function &F, BasicBlock *B, LLVMContext &llvm_context){


  //llvm::Constant* init = llvm::ConstantDataArray::get(llvm_context, sizes);
  //GlobalVariable *gvar = new GlobalVariable(init->getType(),true,GlobalValue::CommonLinkage,init,"sizes");
  //Instruction* loadInst_global =  new LoadInst(init->getType(),M.getGlobalVariable("sizes"));
  //new StoreInst(init,M.getGlobalVariable("sizes"),B->getTerminator());
  M.getGlobalVariable("parallel_size")->setInitializer(llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(B->getContext()),sizes_2.size(), false));
  GlobalVariable* G_ref_array = M.getGlobalVariable("parallel_arr_size");
  Type* T_arr = G_ref_array->getType();
  Type* T_arr_1 = T_arr->getContainedType(0); //*int
  Type* T_arr_2 = T_arr_1->getContainedType(0); //int

  GlobalVariable* G_ref_array_loop = M.getGlobalVariable("loop_arr_size");
  Type* T_arr_loop = G_ref_array_loop->getType();
  Type* T_arr_1_loop = T_arr->getContainedType(0); //*int
  Type* T_arr_2_loop = T_arr_1->getContainedType(0); //int

  ConstantInt *sizes_array = ConstantInt::get(Type::getInt64Ty(B->getContext()), sizes_2.size());
  Constant *sizes_array_val = ConstantExpr::getSizeOf(T_arr_2);
  Instruction* malloc_sizes_array = CallInst::CreateMalloc(B->getTerminator(), Type::getInt64Ty(B->getContext()), T_arr_2, sizes_array_val, sizes_array, nullptr, "malloced");
  
  ConstantInt *sizes_array_loop = ConstantInt::get(Type::getInt64Ty(B->getContext()), loop_split_2.size());
  Constant *sizes_array_val_loop = ConstantExpr::getSizeOf(T_arr_2_loop);
  Instruction* malloc_sizes_array_loop = CallInst::CreateMalloc(B->getTerminator(), Type::getInt64Ty(B->getContext()), T_arr_2_loop, sizes_array_val_loop, sizes_array_loop, nullptr, "malloced");
  

  if(Value* v = dyn_cast<Value>(malloc_sizes_array)) {
    errs()<<"---------------------- Global variable is a value type ------------------------- \n";
  }

  else {
    errs()<<"--------------------- not a value type --------------------------- \n";
  }

  std::string str1, str2;
  raw_string_ostream stream1(str1), stream2(str2);
  // malloc_sizes_array->getType()->print(stream1,false);
  M.getGlobalVariable("parallel_arr_size")->getType()->print(stream2, false);

  errs()<<"type of arg 1 and 2 is: " <<str1<<" "<<str2<<"\n";

  Instruction *store_sizes_array = new StoreInst(malloc_sizes_array, M.getGlobalVariable("parallel_arr_size"), B->getTerminator());
  Instruction *store_sizes_array_loop = new StoreInst(malloc_sizes_array_loop, M.getGlobalVariable("loop_arr_size"), B->getTerminator());

  errs()<<"------------------- Store Instruction Complete ---------------------------\n";

  Instruction *load_sizes_array = new LoadInst(T_arr_1, M.getGlobalVariable("parallel_arr_size"), "", B->getTerminator());
  Instruction *load_sizes_array_loop = new LoadInst(T_arr_1_loop, M.getGlobalVariable("loop_arr_size"), "", B->getTerminator());

  errs()<<"------------------- load Instruction Complete ---------------------------\n";

  for(int i = 0 ; i < sizes_2.size() ; i++){
    std::vector<llvm::Value*> sizes_indices;
    sizes_indices.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),i, false));
    GetElementPtrInst *sizes_gepinst = GetElementPtrInst::Create(T_arr_2, load_sizes_array, sizes_indices, "", B->getTerminator());
    sizes_gepinst->getType()->print(stream1,false);
    errs()<<"gepinst inst type is: "<<str1<<"\n";
    new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, sizes_2[i], true)),sizes_gepinst,B->getTerminator());
  }

  for(int i = 0 ; i < loop_split_2.size() ; i++){
    std::vector<llvm::Value*> sizes_indices;
    sizes_indices.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),i, false));
    GetElementPtrInst *sizes_gepinst = GetElementPtrInst::Create(T_arr_2_loop, load_sizes_array_loop, sizes_indices, "", B->getTerminator());
    new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, loop_split_2[i].size(), true)),sizes_gepinst,B->getTerminator());
  }

  // Above done storing the sizes array

  errs()<<"------------ loop completion ---------------"<<"\n";

  GlobalVariable* G = M.getGlobalVariable("parallel_region");
  Type* T = G->getType(); //***details
  Type* T_1 = T->getContainedType(0); //**details
  Type* T_2 = T_1->getContainedType(0); //*details
  Type* T_3 = T_2->getContainedType(0); //details

  GlobalVariable* G_loop = M.getGlobalVariable("loop_execution");
  Type* T_loop = G_loop->getType(); //***details
  Type* T_1_loop = T_loop->getContainedType(0); //**details
  Type* T_2_loop = T_1_loop->getContainedType(0); //*details
  Type* T_3_loop = T_2_loop->getContainedType(0); //details

  ConstantInt *arraysize_para_region = ConstantInt::get(Type::getInt64Ty(B->getContext()), astdata_2.size());
  Constant* allocsize_para_region = ConstantExpr::getSizeOf(T_2);
  // allocsize_para_region = ConstantExpr::getTruncOrBitCast(allocsize_para_region, Type::getInt64Ty(B->getContext()));
  Instruction *malloced_para_region = CallInst::CreateMalloc(B->getTerminator(), Type::getInt64Ty(B->getContext()), T_2, allocsize_para_region, arraysize_para_region, nullptr, "malloced");
  
  // errs()<<"--------------------------- printing sizes---------------------------"<<"\n";

  // std::string temp_string;
  // raw_string_ostream check(temp_string);M.getGlobalVariable("ast_size")
  // allocsize_para_region->print(check);
  // errs()<<"size of T_2 T_3 malloc_parallel= "<<M.getDataLayout().getTypeAllocSize(T_2)<<" "<<M.getDataLayout().getTypeAllocSize(T_3)<<" "<<M.getDataLayout().getTypeAllocSize(malloced_para_region->getType()->getContainedType(0))<<"\n";   //datalayout class is used in llvm
  Instruction *store_para_region = new StoreInst(malloced_para_region,M.getGlobalVariable("parallel_region"),B->getTerminator());
  // Instruction *load_para_region = new LoadInst(T_1,M.getGlobalVariable("parallel_region"),"",B->getTerminator());

  /*
    Global storage for loop split info below
  */

  ConstantInt *arraysize_para_region_loop = ConstantInt::get(Type::getInt64Ty(B->getContext()), loop_split_2.size());
  Constant* allocsize_para_region_loop = ConstantExpr::getSizeOf(T_2_loop);
  Instruction *malloced_para_region_loop = CallInst::CreateMalloc(B->getTerminator(), Type::getInt64Ty(B->getContext()), T_2_loop, allocsize_para_region_loop, arraysize_para_region_loop, nullptr, "malloced");
  Instruction *store_para_region_loop = new StoreInst(malloced_para_region_loop,M.getGlobalVariable("loop_execution"),B->getTerminator());

  for(int i = 0 ; i < loop_split_2.size() ; i++){
    ConstantInt *arraysize = ConstantInt::get(Type::getInt64Ty(B->getContext()), loop_split_2[i].size());
    Constant* allocsize = ConstantExpr::getSizeOf(T_3_loop);
    //allocsize = ConstantExpr::getTruncOrBitCast(allocsize, Type::getInt64Ty(B->getContext()));
    //ConstantInt* allocsize_new = ConstantInt::get(Type::getInt64Ty(B->getContext()), M.getDataLayout().getTypeAllocSize(T_3));
    Instruction *malloc_sub_region = CallInst::CreateMalloc(B->getTerminator(), Type::getInt64Ty(B->getContext()), T_3_loop, allocsize, arraysize, nullptr, "malloced");
    // Instruction *loadMalloced = new LoadInst();
    Instruction *load_para_region = new LoadInst(T_1_loop,M.getGlobalVariable("loop_execution"),"",B->getTerminator());
    //errs()<<"Malloced size is = "<<M.getDataLayout().getTypeAllocSize(malloced->getType()->getContainedType(0))<<"\n";
    std::vector<llvm::Value*> indices_parallel;
    indices_parallel.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),i, false));
    GetElementPtrInst *gepinst_1 = GetElementPtrInst::Create(T_2_loop,load_para_region,indices_parallel,"",B->getTerminator());
    Instruction *store_sub_region = new StoreInst(malloc_sub_region,gepinst_1,B->getTerminator());
    Instruction *load_para_region_1 = new LoadInst(T_1_loop,M.getGlobalVariable("loop_execution"),"",B->getTerminator());
  }

  errs()<<"------------- parallel region storage complete ------------------\n";

  for(int i = 0 ; i < astdata_2.size() ; i++){
    ConstantInt *arraysize = ConstantInt::get(Type::getInt64Ty(B->getContext()), astdata_2[i].size());
    Constant* allocsize = ConstantExpr::getSizeOf(T_3);
    //allocsize = ConstantExpr::getTruncOrBitCast(allocsize, Type::getInt64Ty(B->getContext()));
    //ConstantInt* allocsize_new = ConstantInt::get(Type::getInt64Ty(B->getContext()), M.getDataLayout().getTypeAllocSize(T_3));
    Instruction *malloc_sub_region = CallInst::CreateMalloc(B->getTerminator(), Type::getInt64Ty(B->getContext()), T_3, allocsize, arraysize, nullptr, "malloced");
    // Instruction *loadMalloced = new LoadInst();
    Instruction *load_para_region = new LoadInst(T_1,M.getGlobalVariable("parallel_region"),"",B->getTerminator());
    //errs()<<"Malloced size is = "<<M.getDataLayout().getTypeAllocSize(malloced->getType()->getContainedType(0))<<"\n";
    std::vector<llvm::Value*> indices_parallel;
    indices_parallel.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),i, false));
    GetElementPtrInst *gepinst_1 = GetElementPtrInst::Create(T_2,load_para_region,indices_parallel,"",B->getTerminator());
    Instruction *store_sub_region = new StoreInst(malloc_sub_region,gepinst_1,B->getTerminator());
    Instruction *load_para_region_1 = new LoadInst(T_1,M.getGlobalVariable("parallel_region"),"",B->getTerminator());
    // errs()<<"checking"<<"\n";
    //Instruction *loadinfo = new LoadInst(T_2,M.getGlobalVariable("info"),"",B->getTerminator());
    //errs()<<"ASTDATA SIZE IS "<<astdata.size()<<"\n";

    errs()<<"----------------- parallel region "<<i<<" storage works\n";

    for(int j = 0 ; j < astdata_2[i].size() ; j++){
      //errs()<<"ast value is "<<astdata[i][j]<<" "<<i<<" "<<j<<"\n";
      std::vector<llvm::Value*> indices;
      indices.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),i, false));
      GetElementPtrInst *gepinst_2 = GetElementPtrInst::Create(T_2, load_para_region_1, indices,"",B->getTerminator());
      Instruction *load_sub_para_region = new LoadInst(T_2, gepinst_2,"",B->getTerminator());

      //errs()<<"checking for error"<<"\n";

      std::vector<llvm::Value*> indices_2;
      indices_2.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),j, false));
      GetElementPtrInst *gepinst_3 = GetElementPtrInst::Create(T_3,load_sub_para_region,indices_2,"",B->getTerminator());

      //errs()<<"checking for error2"<<"\n";

      std::vector<llvm::Value*> indices_3;
      indices_3.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),0, false));
      indices_3.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(B->getContext()),0, false)); // this i64 will give access error as struct value accessed is int
      GetElementPtrInst *gepinst_4 = GetElementPtrInst::Create(T_3,gepinst_3,indices_3,"",B->getTerminator());
      new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, astdata_2[i][j].first, true)),gepinst_4,B->getTerminator());
      // use option to see if maxvul is set

      // errs()<<"checking for error3"<<"\n";

      // std::vector<llvm::Value*> indices_4;
      // indices_4.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),0, false));
      // indices_4.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(B->getContext()),1, false)); // this i64 will give access error as struct value accessed is int
      // GetElementPtrInst *gepinst_5 = GetElementPtrInst::Create(T_3,gepinst_3,indices_4,"",B->getTerminator());
      // new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, astdata[i][j].second , true)),gepinst_5,B->getTerminator());

      // // load and read its value

      errs()<<"checking for error4"<<"\n";

      // std::vector<llvm::Value*> indices_5;
      // indices_5.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),0, false));
      // indices_5.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(B->getContext()),2, false)); // this i64 will give access error as struct value accessed is int
      // GetElementPtrInst *gepinst_6 = GetElementPtrInst::Create(T_3,gepinst_3,indices_5,"",B->getTerminator());
      // new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, i+1, false)),gepinst_6,B->getTerminator());
    
      // errs()<<"checking for error5"<<"\n";
    }
  }
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
                errs()<<"Function is also not omp_outlined\n";
                std::vector<Loop*> temp_loops = retrieveLoopsFunc(*CalledFunc,MA);
                allLoops_2.insert(allLoops_2.end(),temp_loops.begin(),temp_loops.end());
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

std::vector<int> splittingLoops(Function &F, int parallel_region_id, ModuleAnalysisManager &MA){
  std::vector<int> temp;
  
  llvm::Module *M = F.getParent();
  llvm::LLVMContext &CTX = M->getContext();
  std::vector<Loop*> allLoops_2 = retrieveLoopsFunc(F,MA);
  for(int i = 0 ; i < allLoops_2.size(); i++) {
    Loop* ltemp = allLoops_2[i];
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

    bool split_check = LoopSplit_2(ltemp, 2, parallel_region_id, loop_counter_2, loop_counter_2);
    if(split_check){
      llvm::Value *set_loop_id =  llvm::ConstantInt::get(llvm::Type::getInt32Ty(CTX),loop_counter_2);
      llvm::MDNode* loop_id_value = llvm::MDNode::get(CTX, llvm::ValueAsMetadata::get(set_loop_id));
      //ltemp->setLoopID(loop_id_value);
      temp.push_back(loop_counter_2);
      loop_counter_2++;
    }
  }

  return temp;
}

int setAstData_2(Module &M, Function &F, LLVMContext &CTX, int ctr, int pid, BasicBlock &callblock){

  std::vector<std::pair<int,int> > temp;
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
        for(unsigned i = 0 ; i < n ; i++){
          temp.push_back(std::make_pair(init->getElementAsInteger(i),init_sub->getElementAsInteger(i)));
        }
        //errs()<<"Retreiving num elements for each outlined "<<temp.size()<<"\n";
      }
    }
  }

  astdata_2[pid] = temp;
  sizes_2[pid] = temp.size();
}

std::string dumptest_2;
raw_string_ostream dumpdata_2(dumptest_2);

void updateWorkId_2(Module &M, Function &F, LLVMContext &CTX, ModuleAnalysisManager &MA){
  int ctr = 1, p_id;
  MDNode* parallel_id = F.getMetadata("parallel_id");
  Value* parallel_id_temp = dyn_cast<ValueAsMetadata>(parallel_id->getOperand(0))->getValue();
  auto* ci = dyn_cast<ConstantInt>(parallel_id_temp);
  p_id = ci->getZExtValue()-1;

  for (Function::iterator block_iter = F.begin(), block_iter_end = F.end(); block_iter != block_iter_end; ++block_iter) {
    BasicBlock &B = *block_iter;
    for(BasicBlock::iterator instr_iter = B.begin(), instr_iter_end = B.end(); instr_iter != instr_iter_end; ++instr_iter){
      Instruction &I = *instr_iter;
      if(CallInst* call_inst = dyn_cast<CallInst>(&I)){
        Function* fn = call_inst->getCalledFunction();
        //errs()<<fn->dump()<<"\n";
        call_inst->print(dumpdata_2,false);
        errs()<<"call instruction is "<<dumptest_2<<"\n";
        if(fn){
            if(fn->getName() == "__kmpc_for_static_init_4"){
                llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
                call_inst->setOperand(9,itr_ci);
                setAstData_2(M,F,CTX,ctr,p_id,B);
                ctr++;
            }

            else if(fn->getName() == "__kmpc_single"){
                llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
                call_inst->setOperand(2,itr_ci);
                setAstData_2(M,F,CTX,ctr,p_id,B);
                ctr++;
            }
        }
      }
    }
  }

  /*loop split code*/

  if(maxvuln_set_2){
    loop_split_2[p_id] = splittingLoops(F,p_id,MA);
  }

  /*end of loop split code*/
}

PreservedAnalyses TtexUpdatePassV2::run(Module &M, ModuleAnalysisManager &MA) {

    LLVMContext &CTX = M.getContext();

    int counter = 0;

    for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
      Function &F = *func_iter;

      if (!F.isDeclaration()) {
          //errs()<<"Function name is:"<<F.getName()<<"\n";
        if(F.getName().contains(".omp_outlined.") && F.getName() != ".omp_outlined._debug__"){
          counter++;
        }
      }
    }

    std::vector< std::pair<int,int> > temp;
    std::vector<int> loop_split_temp;
    std::vector< std::vector<int> > temp_loop_split(counter,loop_split_temp);
    std::vector<int> sizes_temp(counter,0);
    std::vector< std::vector< std::pair<int,int> > >astdata_temp(counter,temp);

    // now we could go through all omp_outlined again read metadata and parallel id and assign values according to id's
    // However clang parses the same way as we check in the pass that is every function in a module so no need for above

    sizes_2 = sizes_temp;
    astdata_2 = astdata_temp;
    loop_split_2 = temp_loop_split;


    for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
    
      Function &F = *func_iter;

      if (!F.isDeclaration()) {
        errs()<<"Function name is:"<<F.getName()<<"\n";
        if(F.getName().contains(".omp_outlined.") && F.getName() != ".omp_outlined._debug__"){
          //errs()<<"omp outlined function called\n";
          //errs()<<"Function name is:"<<F.getName()<<"\n";
          updateWorkId_2(M,F,CTX,MA);
        }
      }
    }

    for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
      Function &F = *func_iter;

      if (!F.isDeclaration()) {
        errs()<<"Function name is:"<<F.getName()<<"\n";
        if(F.getName().contains("ompt_start_tool")){
          BasicBlock* B;
          B = &*(F.begin());
          setLookUpTable_2(M,F,B,CTX);
          break;
        }
      }
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