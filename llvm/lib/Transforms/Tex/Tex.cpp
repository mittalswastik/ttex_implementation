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
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/LoopPeel.h"
#include "llvm/Transforms/Utils/LoopSimplify.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/SizeOpts.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Attributes.inc"
#include <iostream>
#include <bits/stdc++.h>
#include "llvm/Transforms/Scalar/Ttex.h"
#include "llvm/IR/DataLayout.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;

// namespace llvm {
//   void initializeTtexPassPass (PassRegistry&);
// } // end namespace llvm

std::vector< std::vector< std::pair<int,int> > > astdata; //storing the sub region info -- but need to fix the id's to correct location
std::vector<int> sizes;

bool maxvuln_set = true;

#define omp_for_ref -1
#define omp_sections_ref 0
#define omp_single_ref -2

void LoopSplit(Loop *L, unsigned count, BasicBlock *ExitBlock, int parallel_id, int sub_id){

  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *LatchBlock = L->getLoopLatch();
  // BasicBlock *ExitingBlock = L->getExitingBlock();
  //BasicBlock *ExitBlock;

  std::vector<BasicBlock *> OriginalLoopBlocks = L->getBlocks();
  std::vector<BasicBlock *> newLoopBlocks;
  
  // loop below retreives all the other blocks of the original loop which are not exit latch or header

  for(int i = 0 ; i < OriginalLoopBlocks.size() ; i++){
    // OriginalLoopBlocks[i]->printAsOperand(errs(),false);
    std::string temp;
    raw_string_ostream temp_stream(temp);
    OriginalLoopBlocks[i]->printAsOperand(temp_stream,false);
    temp_stream.flush();
    if(OriginalLoopBlocks[i] != Header && OriginalLoopBlocks[i] != LatchBlock){
      std::cout<<"block added to new loop blocks "<<temp<<std::endl;
      newLoopBlocks.push_back(OriginalLoopBlocks[i]); // got blocks for new loop
    }
  }

  llvm::Function* Func = Header->getParent();
  llvm::Module *M = Func->getParent();
  llvm::LLVMContext &CTX = M->getContext();
  llvm::BasicBlock* InnerLoopPreheader = BasicBlock::Create(CTX, "SplitLoopPreheader", Func); 
  llvm::BasicBlock* InnerLoopHeader = BasicBlock::Create(CTX, "SplitLoopHeader", Func);
  llvm::BasicBlock* InnerLoopLatch = BasicBlock::Create(CTX, "SplitLoopLatch", Func);

  /*assinging iteration range*/

  //llvm::ConstantInt *start_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),0, false); // starting value
  llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),1, false); // iteration value
  llvm::ConstantInt *end_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),count-1, false);
  llvm::ConstantInt *end_ci_outer = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),count, false);

  // create new loop

  /*
   Add one header block and let the last block in the newloopblocks be the latch and exiting block

  Preheader: 1. for allocation of induction variable memory

  Header:  1. would have a phinode from H/EG block and Latch of the new loop
           2. all add the induction variable here

  Latch:   1. Add the add 1 to induction variable just before last jump instruction which either jumps to new header or the latch of old loop
           2. This would require changing the last jump instruction

  *** we dont need additional latch block 
  */

  /*retreive the result of icmp instruction and also stored induction variable of the original header*/

  llvm::Value* temp_v;
  llvm::Value* originalInd;

  llvm::BasicBlock::iterator indInst = Header->begin(); // this will always be loading start value

  if(isa<llvm::LoadInst> (indInst)){
    // this should always be true
    llvm::Instruction *temp_i = &*indInst;
    originalInd = temp_i->getOperand(0);
  }

  else {
    std::cout<<"XXXXXXXXXXXXXXXXXXXXXx wrong loop format: (Header always loads ind var first) xXXXXXXXXXXXXXXXXXXXXXXXXXXXX"<<std::endl;
    return;
  }



  for(llvm::BasicBlock::iterator I = Header->begin(), Iend = Header->end(); I != Iend ; ++I){
    if (isa <llvm::ICmpInst> (I)){
      std::cout<<"found compare instruction"<<std::endl;
      temp_v = &*I; // iterator to instruction
    }
  }

  /*Header block*/

  // load and store induction variable values

  IRBuilder<> prehead(InnerLoopPreheader);
  llvm::LoadInst* load_original = prehead.CreateLoad(llvm::IntegerType::getInt32Ty(CTX), originalInd, "");
  llvm::AllocaInst* allocate_start = prehead.CreateAlloca(llvm::IntegerType::getInt32Ty(CTX), nullptr ,"iterator");
  llvm::AllocaInst* allocate_end = prehead.CreateAlloca(llvm::IntegerType::getInt32Ty(CTX),nullptr, "iterator_bound");
  llvm::StoreInst* store_start = prehead.CreateStore(load_original,allocate_start,false);
  // llvm::LoadInst* load_start =  prehead.CreateLoad(llvm::IntegerType::getInt64Ty(CTX), allocate_start, "");
  llvm::Value* ind_end = prehead.CreateNSWAdd(end_ci, load_original,"");
  prehead.CreateStore(ind_end, allocate_end, false);
  prehead.CreateBr(InnerLoopHeader);

  IRBuilder<> head(InnerLoopHeader);
  llvm::LoadInst* load_ind = head.CreateLoad(llvm::IntegerType::getInt32Ty(CTX),allocate_start,"inner_itr_start");
  llvm::LoadInst* load_ind_end = head.CreateLoad(llvm::IntegerType::getInt32Ty(CTX),allocate_end,"inner_itr_end");
  // llvm::LoadInst* load_old_ind = head.CreateLoad(llvm::IntegerType::getInt32Ty(CTX), originalInd, "outer_itr");
  // llvm::BinaryOperator* new_ind = head.CreateNSWAdd(load_start,load_old_ind,"induction_value");
  llvm::Value* compare = head.CreateICmpSLE(load_ind,load_ind_end);
  head.CreateCondBr(compare, newLoopBlocks[0], LatchBlock); // if true execute the original body of loop else move to latch of original

  // change the branch instruction of last block of newloopblocks also add 1 to iterator and also the original header (also value of the header)

  llvm::Instruction *Linst = Header->getTerminator();
  if(Linst){
    Linst->eraseFromParent();
    // llvm::Instruction *Linst_temp = Header->getTerminator();
    // if(Linst_temp){ // this will remain NULL not the previous Instruction
    //   cout<<"works"<<endl;
    //   //Value* v = LI->getOperand(0);  
    // }

    llvm::BranchInst::Create(InnerLoopPreheader,ExitBlock,temp_v,Header);
    //llvm::BranchInst::Create(InnerLoopPreheader,Header);
    //Header->getInstList.push_back(I)
  }

  for(llvm::BasicBlock::iterator I = LatchBlock->begin(), Iend = LatchBlock->end(); I != Iend ; ++I){
    if (isa <llvm::BinaryOperator> (I)){
      std::cout<<"found binary operator instruction"<<std::endl;
      //int count = 0;
      for(llvm::BinaryOperator::op_iterator oi = I->op_begin(), oi_end = I->op_end(); oi != oi_end; ++oi){
        if(isa<llvm::ConstantInt>(oi)){
          std::cout<<"constant value found"<<std::endl;
          oi->set(end_ci_outer);
          //I->setOperand(count,end_ci);
          llvm::ConstantInt *ci = cast<llvm::ConstantInt>(oi);
          errs()<<"constant value"<<ci->getValue()<<"\n";
          //oi->set(end_ci);
          break;
        }

        // else{
        //   count++;
        // }
      }
      break;
    }
  }

  // // insert ompt function in the latch of the outer loop (can be just before the terminator instructions)

  FunctionType *testing = FunctionType::get(
      Type::getVoidTy(CTX),
      {IntegerType::getInt32Ty(CTX), IntegerType::getInt32Ty(CTX), IntegerType::getInt32Ty(CTX)},
      //PointerType::getPointerAddressSpace(),
      /*IsVarArgs=*/false);

  FunctionCallee hookTest = M->getOrInsertFunction("ompt_test", testing);
  //Function *hook = dyn_cast<Function>(hookTest.getCallee());
  std::vector<Value*> args;
  ConstantInt *arg1 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),parallel_id, false);
  ConstantInt *arg2 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),sub_id, false);
  ConstantInt *arg3 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),-1, false);
  args.push_back(arg1);
  args.push_back(arg2);
  args.push_back(arg3);
  llvm::CallInst::Create(hookTest,args,"",&*(--LatchBlock->end()));

  llvm::Instruction *Linst_2 = newLoopBlocks[newLoopBlocks.size()-1]->getTerminator();
  if(Linst_2){
    Linst_2->eraseFromParent();
    // llvm::LoadInst *ind_var = new llvm::LoadInst(llvm::IntegerType::getInt32Ty(CTX),allocate_start,"store_start",newLoopBlocks[newLoopBlocks.size()-1]);
    // llvm::BinaryOperator *bio = llvm::BinaryOperator::CreateNSW(llvm::Instruction::Add,ind_var,itr_ci,"increment");
    // newLoopBlocks[newLoopBlocks.size()-1]->getInstList().push_back(bio);
    // llvm::StoreInst *ind_var_store = new llvm::StoreInst(bio,allocate_start,newLoopBlocks[newLoopBlocks.size()-1]);
    llvm::BranchInst::Create(InnerLoopLatch,newLoopBlocks[newLoopBlocks.size()-1]);
  }

  std::cout<<"----------------------------------works till here--------------------"<<std::endl;

  IRBuilder<> latch(InnerLoopLatch);
  llvm::LoadInst *ind_var = latch.CreateLoad(llvm::IntegerType::getInt32Ty(CTX),allocate_start,"store_start");
  std::cout<<"----------------------------------works till here--------------------"<<std::endl;
  llvm::Value* bio = latch.CreateNSWAdd(ind_var, itr_ci,"increment");
  latch.CreateStore(bio,allocate_start,false);
  latch.CreateBr(InnerLoopHeader);  // this is created to maintain systematic loop formation of llvm

  for(int i = 0 ; i < newLoopBlocks.size(); i++){
    for(llvm::BasicBlock::iterator I = newLoopBlocks[i]->begin(), Iend = newLoopBlocks[i]->end(); I != Iend ; ++I){
      if(isa<llvm::LoadInst> (I)){
        if(I->getOperand(0) == originalInd){
          llvm::Value *val = &*I;
          I->setOperand(0,allocate_start);
        }
      }
    }
  }

  BranchInst *LatchBI = dyn_cast<BranchInst>(LatchBlock->getTerminator());
  
  BranchInst *ExitingBI = nullptr;
  bool LatchIsExiting = L->isLoopExiting(LatchBlock);
  if (LatchIsExiting)
  ExitingBI = LatchBI;
  else if (BasicBlock *ExitingBlock = L->getExitingBlock())
  ExitingBI = dyn_cast<BranchInst>(ExitingBlock->getTerminator());
}

void setLookUpTable(Module &M, Function &F, BasicBlock *B, LLVMContext &llvm_context){


  //llvm::Constant* init = llvm::ConstantDataArray::get(llvm_context, sizes);
  //GlobalVariable *gvar = new GlobalVariable(init->getType(),true,GlobalValue::CommonLinkage,init,"sizes");
  //Instruction* loadInst_global =  new LoadInst(init->getType(),M.getGlobalVariable("sizes"));
  //new StoreInst(init,M.getGlobalVariable("sizes"),B->getTerminator());
  M.getGlobalVariable("parallel_size")->setInitializer(llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(B->getContext()),sizes.size(), false));
  GlobalVariable* G_ref_array = M.getGlobalVariable("ast_size");
  Type* T_arr = G_ref_array->getType();
  Type* T_arr_1 = T_arr->getContainedType(0); //*int
  Type* T_arr_2 = T_arr_1->getContainedType(0); //int

  ConstantInt *sizes_array = ConstantInt::get(Type::getInt64Ty(B->getContext()), sizes.size());
  Constant *sizes_array_val = ConstantExpr::getSizeOf(T_arr_2);
  Instruction* malloc_sizes_array = CallInst::CreateMalloc(B->getTerminator(), Type::getInt64Ty(B->getContext()), T_arr_2, sizes_array_val, sizes_array, nullptr, "malloced");
  
  if(Value* v = dyn_cast<Value>(malloc_sizes_array)) {
    errs()<<"---------------------- Global variable is a value type ------------------------- \n";
  }

  else {
    errs()<<"--------------------- not a value type --------------------------- \n";
  }

  std::string str1, str2;
  raw_string_ostream stream1(str1), stream2(str2);
  // malloc_sizes_array->getType()->print(stream1,false);
  M.getGlobalVariable("ast_size")->getType()->print(stream2, false);

  errs()<<"type of arg 1 and 2 is: " <<str1<<" "<<str2<<"\n";

  Instruction *store_sizes_array = new StoreInst(malloc_sizes_array, M.getGlobalVariable("ast_size"), B->getTerminator());

  errs()<<"------------------- Store Instruction Complete ---------------------------\n";

  Instruction *load_sizes_array = new LoadInst(T_arr_1, M.getGlobalVariable("ast_size"), "", B->getTerminator());

  errs()<<"------------------- load Instruction Complete ---------------------------\n";

  for(int i = 0 ; i < sizes.size() ; i++){
    std::vector<llvm::Value*> sizes_indices;
    sizes_indices.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),i, false));
    GetElementPtrInst *sizes_gepinst = GetElementPtrInst::Create(T_arr_2, load_sizes_array, sizes_indices, "", B->getTerminator());
    sizes_gepinst->getType()->print(stream1,false);
    errs()<<"gepinst inst type is: "<<str1<<"\n";
    new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, sizes[i], true)),sizes_gepinst,B->getTerminator());
  }

  // Above done storing the sizes array

  errs()<<"------------ loop completion ---------------"<<"\n";

  GlobalVariable* G = M.getGlobalVariable("parallel_region");
  Type* T = G->getType(); //***details
  Type* T_1 = T->getContainedType(0); // **details
  Type* T_2 = T_1->getContainedType(0); // *details
  Type* T_3 = T_2->getContainedType(0); // details

  ConstantInt *arraysize_para_region = ConstantInt::get(Type::getInt64Ty(B->getContext()), astdata.size());
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

  errs()<<"------------- parallel region storage complete ------------------\n";

  for(int i = 0 ; i < astdata.size() ; i++){
    ConstantInt *arraysize = ConstantInt::get(Type::getInt64Ty(B->getContext()), astdata[i].size());
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

    for(int j = 0 ; j < astdata[i].size() ; j++){
      //errs()<<"ast value is "<<astdata[i][j]<<" "<<i<<" "<<j<<"\n";
      std::vector<llvm::Value*> indices;
      indices.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),i, false));
      GetElementPtrInst *gepinst_2 = GetElementPtrInst::Create(T_2, load_para_region_1, indices,"",B->getTerminator());
      Instruction *load_sub_para_region = new LoadInst(T_2, gepinst_2,"",B->getTerminator());

      //errs()<<"checking for error"<<"\n";

      std::vector<llvm::Value*> indices_2;
      indices.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),j, false));
      GetElementPtrInst *gepinst_3 = GetElementPtrInst::Create(T_3,load_sub_para_region,indices_2,"",B->getTerminator());

      //errs()<<"checking for error2"<<"\n";

      std::vector<llvm::Value*> indices_3;
      indices_3.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),0, false));
      indices_3.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(B->getContext()),0, false)); // this i64 will give access error as struct value accessed is int
      GetElementPtrInst *gepinst_4 = GetElementPtrInst::Create(T_3,gepinst_3,indices_3,"",B->getTerminator());
      new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, astdata[i][j].first, true)),gepinst_4,B->getTerminator());
      // use option to see if maxvul is set

      // errs()<<"checking for error3"<<"\n";

      std::vector<llvm::Value*> indices_4;
      indices_4.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),0, false));
      indices_4.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(B->getContext()),1, false)); // this i64 will give access error as struct value accessed is int
      GetElementPtrInst *gepinst_5 = GetElementPtrInst::Create(T_3,gepinst_3,indices_4,"",B->getTerminator());
      new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, astdata[i][j].second , true)),gepinst_5,B->getTerminator());

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

void protectSections(Module &M, Function &F, LLVMContext &CTX, BasicBlock& callblock, int parallel_region_id, int sub_region_id){
  // need loop split for all the loops within the section - same code as protectFor
}

void protectFor(Module &M, Function &F, LLVMContext &CTX, BasicBlock& callblock, int parallel_region_id, int sub_region_id){
  DominatorTree DT = llvm::DominatorTree();
  DT.recalculate(F);
  LoopInfoBase<BasicBlock, Loop>* LInfo = new llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>();
  LInfo->releaseMemory();
  LInfo->analyze(DT);

  errs()<<"checking for errors"<<"\n";

  for(LoopInfoBase<BasicBlock, Loop>::iterator loop_iter = LInfo->begin(), loop_iter_end = LInfo->end(); loop_iter != loop_iter_end; ++loop_iter){
    //testing
    Loop *ltemp = *loop_iter;
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

    // std::string h_label;
    // raw_string_ostream stream2(h_label);

    // if(header){
    //   errs()<<"header found"<<"\n";
    //   header->printAsOperand(stream2,false);
    //   errs()<<h_label<<"\n";
    // }

    errs()<<"Found a loop"<<"\n";

    if(BranchInst *binst = dyn_cast<BranchInst>(header->getTerminator())){
      if(BasicBlock *compare_block = dyn_cast<BasicBlock> (binst->getOperand(1))){ // exiting block of the loop
        if(BranchInst *binst_2 = dyn_cast<BranchInst>(compare_block->getTerminator())){
          if(BasicBlock *compare_block_2 = dyn_cast<BasicBlock> (binst_2->getOperand(0))){
            errs()<<"checking for the loop"<<"\n";
            std::string check_string;
            raw_string_ostream check_stream(check_string);
            compare_block_2->printAsOperand(check_stream,false);
            errs()<<check_string<<"\n";
            for(BasicBlock::iterator instr_iter = compare_block_2->begin(), instr_iter_end = compare_block_2->end(); instr_iter != instr_iter_end; ++instr_iter){
              Instruction &I = *instr_iter;
              if(CallInst* call_inst = dyn_cast<CallInst>(&I)){
                Function* fn = call_inst->getCalledFunction();
                if(fn->getName() == "__kmpc_for_static_fini"){
                  errs()<<"found the right loop"<<"\n";
                  //loop split here
                  LoopSplit(ltemp, 2, compare_block, parallel_region_id, sub_region_id);
                }
              }
            }
          }
        }

        // for (Function::iterator block_iter = block_iteration, block_iter_end = F.end(); block_iter != block_iter_end; ++block_iter){
        //   BasicBlock &B = *block_iter;
        //   if(&B == header){
        //     errs()<<"found the right loop"<<"\n"; //GG
        //     //add loop split code here and done!
        //     return; // if loop found then split and return otherwise loops after this will also be splitted
        //   }

        //   else if(&B == compare_block){
        //     errs()<<"compare block equal to current block\n";
        //     break; // not this loop -- to break early
        //   }
        // }
      }
    }
  }
}

void protectSingle(Module &M, Function &F, LLVMContext &CTX, BasicBlock& callblock, int parallel_region_id, int sub_region_id){

}

void setAstData(Module &M, Function &F, LLVMContext &CTX, int ctr, BasicBlock &callblock){

  std::vector< std::pair<int,int> > temp;
  MDNode* ttex_array = F.getMetadata("ttex_array");
  MDNode* ttex_sub_array = F.getMetadata("ttex_sub_array");
  MDNode* parallel_id = F.getMetadata("parallel_id");

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

  // astdata.push_back(temp);
  // sizes.push_back(temp.size());

  Value* parallel_id_temp = dyn_cast<ValueAsMetadata>(parallel_id->getOperand(0))->getValue();
  auto* ci = dyn_cast<ConstantInt>(parallel_id_temp);

  //errs()<<"------------ parallel id value is------"<<ci->getZExtValue()<<"\n";
  //errs()<<"astdata size and sizes size: "<<astdata.size()<<" "<<sizes.size()<<"\n";

  astdata[ci->getZExtValue()-1] = temp;
  sizes[ci->getZExtValue()-1] = temp.size();

  if(maxvuln_set){
    if(astdata[ci->getZExtValue()-1][ctr-1].first > omp_sections_ref){
      protectSections(M,F,CTX,callblock,ci->getZExtValue()-1,ctr-1);
    }

    else if(astdata[ci->getZExtValue()-1][ctr-1].first == omp_for_ref){
      protectFor(M,F,CTX,callblock,ci->getZExtValue(),ctr);
      Value* max_iteration;
      
      for(BasicBlock::iterator instr_iter = callblock.begin(), instr_iter_end = callblock.end(); instr_iter != instr_iter_end; ++instr_iter){
        Instruction &I = *instr_iter;
        if(CallInst* call_inst = dyn_cast<CallInst>(&I)){
          Function* fn = call_inst->getCalledFunction();
          if(fn->getName() == "__kmpc_for_static_init_4"){
            max_iteration = call_inst->getArgOperand(5);
            errs()<<"max iteration value is: \n";
            max_iteration->print(errs());
            errs()<<"\n";
          }
        }
      }

      for(BasicBlock::iterator instr_iter = callblock.begin(), instr_iter_end = callblock.end(); instr_iter != instr_iter_end; ++instr_iter){
        Instruction &I = *instr_iter;
        if(StoreInst* store_inst = dyn_cast<StoreInst>(&I)){
          if(max_iteration == store_inst->getOperand(1)){
            errs()<<"store instuction found is"<<"\n";
            store_inst->print(errs());
            if (llvm::ConstantInt* CI = dyn_cast<llvm::ConstantInt>(store_inst->getOperand(0))) {
              errs()<<"Value: "<<CI->getZExtValue()<<"\n";
            }
          }
        }
      }      
    }

    else if(astdata[ci->getZExtValue()-1][ctr-1].first == omp_single_ref){
      protectSingle(M,F,CTX,callblock,ci->getZExtValue()-1,ctr-1);
    }
  }

  // if(parallel_id){
  //   Value* parallel_id_temp = dyn_cast<ValueAsMetadata>(parallel_id->getOperand(0))->getValue();
  //   if(parallel_id_temp){
  //     auto* ci = dyn_cast<ConstantInt>(parallel_id_temp);
  //     if(ci){
  //       astdata[ci->getZExtValue()] = temp;
  //       sizes[ci->getZExtValue()] = temp.size();
  //     }
  //   }
  // }
}

std::string dumptest;
raw_string_ostream dumpdata(dumptest);

void updateWorkId(Module &M, Function &F, LLVMContext &CTX){
  int ctr = 1;
  for (Function::iterator block_iter = F.begin(), block_iter_end = F.end(); block_iter != block_iter_end; ++block_iter) {
    BasicBlock &B = *block_iter;
    for(BasicBlock::iterator instr_iter = B.begin(), instr_iter_end = B.end(); instr_iter != instr_iter_end; ++instr_iter){
      Instruction &I = *instr_iter;
      if(CallInst* call_inst = dyn_cast<CallInst>(&I)){
        Function* fn = call_inst->getCalledFunction();
        //errs()<<fn->dump()<<"\n";
        call_inst->print(dumpdata,false);
        errs()<<"call instruction is "<<dumptest<<"\n";
        if(fn){
            if(fn->getName() == "__kmpc_for_static_init_4"){
                llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
                call_inst->setOperand(9,itr_ci);
                setAstData(M,F,CTX,ctr,B);
                ctr++;
            }

            else if(fn->getName() == "__kmpc_single"){
                llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
                call_inst->setOperand(2,itr_ci);
                setAstData(M,F,CTX,ctr,B);
                ctr++;
            }
        }
      }
    }
  }
}

namespace {
  // Hello2 - The second implementation with getAnalysisUsage implemented.
  struct Tex : public ModulePass {

    static char ID; // Pass identification, replacement for typeid
    Tex() : ModulePass(ID) {}

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.addRequired<DominatorTreeWrapperPass>();
      AU.addRequired<LoopInfoWrapperPass>();
      AU.addRequired<ScalarEvolutionWrapperPass>();
      AU.addRequired<AssumptionCacheTracker>();
      AU.addRequired<TargetTransformInfoWrapperPass>();
    }

    bool runOnModule(Module &M) override {

      LLVMContext &CTX = M.getContext();

      int counter = 0;

      for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
        Function &F = *func_iter;

        if (!F.isDeclaration()) {
           //errs()<<"Function name is:"<<F.getName()<<"\n";
          if(F.getName().contains(".omp_outlined.")){
            counter++;
          }
        }
      }

      std::vector< std::pair<int,int> > temp;
      std::vector<int> sizes_temp(counter,0);
      std::vector< std::vector< std::pair<int,int> > >astdata_temp(counter,temp);

      sizes = sizes_temp;
      astdata = astdata_temp;


      for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
        Function &F = *func_iter;

        if (!F.isDeclaration()) {
          errs()<<"Function name is:"<<F.getName()<<"\n";
          if(F.getName().contains(".omp_outlined.")){
            //errs()<<"omp outlined function called\n";
            //errs()<<"Function name is:"<<F.getName()<<"\n";
            updateWorkId(M,F,CTX);
          }
        }
      }

      for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
        Function &F = *func_iter;

        if (!F.isDeclaration()) {
          errs()<<"Function name is:"<<F.getName()<<"\n";
          if(F.getName() == "ompt_start_tool"){
            BasicBlock* B;
            B = &*(F.begin());
            setLookUpTable(M,F,B,CTX);
            break;
          }
        }
      }

      return false;
    }
  };
}

char Tex::ID = 0;
static RegisterPass<Tex> Z("tex", "tex pass");

static RegisterStandardPasses A(
    PassManagerBuilder::EP_EarlyAsPossible,
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM) { PM.add(new Tex()); });

// added to the cmakescalar list as well else will get linker error