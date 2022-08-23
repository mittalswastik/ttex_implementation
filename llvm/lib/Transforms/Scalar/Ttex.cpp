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
using namespace llvm;

std::vector< std::vector<int> >astdata; //storing the sub region info -- but need to fix the id's to correct location
std::vector<int> sizes;

bool maxvuln_set = true;

#define omp_for_ref -1
#define omp_sections_ref 0
#define omp_single_ref -2

void LoopSplit(Loop *L, unsigned count, BasicBlock *ExitBlock, int parallel_id, std::vector<BasicBlock *> OriginalLoopBlocks, std::vector<BasicBlock *> newLoopBlocks){

  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *LatchBlock = L->getLoopLatch();

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
      //{IntegerType::getInt32Ty(CTX), IntegerType::getInt32Ty(CTX), IntegerType::getInt32Ty(CTX)},
      IntegerType::getInt32Ty(CTX),
      //PointerType::getPointerAddressSpace(),
      /*IsVarArgs=*/false);

  FunctionCallee hookTest = M->getOrInsertFunction("ompt_test", testing);
  //Function *hook = dyn_cast<Function>(hookTest.getCallee());
  std::vector<Value*> args;
  ConstantInt *arg1 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),parallel_id, false);
  args.push_back(arg1);
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
  Instruction* malloc_sizes_array = CallInst::CreateMalloc(B->getTerminator(), Type::getInt64Ty(B->getContext()), T_arr_1, sizes_array_val, sizes_array, nullptr, "malloced");
  Instruction *store_sizes_array = new StoreInst(malloc_sizes_array,M.getGlobalVariable("ast_size"),B->getTerminator());
  Instruction *load_sizes_array = new LoadInst(T_arr_1, M.getGlobalVariable("ast_size"), "", B->getTerminator());

  for(int i = 0 ; i < sizes.size() ; i++){
    std::vector<llvm::Value*> sizes_indices;
    sizes_indices.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),i, false));
    GetElementPtrInst *sizes_gepinst = GetElementPtrInst::Create(T_arr_2, load_sizes_array, sizes_indices, "", B->getTerminator());
    new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, sizes[i], true)),sizes_gepinst,B->getTerminator());
  }

  // Above done storing the sizes array

  GlobalVariable* G = M.getGlobalVariable("parallel_region");
  Type* T = G->getType(); //***details
  Type* T_1 = T->getContainedType(0); // **details
  Type* T_2 = T_1->getContainedType(0); // *details
  Type* T_3 = T_2->getContainedType(0); // details

  ConstantInt *arraysize_para_region = ConstantInt::get(Type::getInt64Ty(B->getContext()), astdata.size());
  Constant* allocsize_para_region = ConstantExpr::getSizeOf(T_2);
  //allocsize_para_region = ConstantExpr::getTruncOrBitCast(allocsize_para_region, Type::getInt64Ty(B->getContext()));
  Instruction *malloced_para_region = CallInst::CreateMalloc(B->getTerminator(), Type::getInt64Ty(B->getContext()), T_1, allocsize_para_region, arraysize_para_region, nullptr, "malloced");
  
 // errs()<<"--------------------------- printing sizes---------------------------"<<"\n";

  // std::string temp_string;
  // raw_string_ostream check(temp_string);
  // allocsize_para_region->print(check);
  //errs()<<"size of T_2 T_3 malloc_parallel= "<<M.getDataLayout().getTypeAllocSize(T_2)<<" "<<M.getDataLayout().getTypeAllocSize(T_3)<<" "<<M.getDataLayout().getTypeAllocSize(malloced_para_region->getType()->getContainedType(0))<<"\n";   //datalayout class is used in llvm
  Instruction *store_para_region = new StoreInst(malloced_para_region,M.getGlobalVariable("parallel_region"),B->getTerminator());
  //Instruction *load_para_region = new LoadInst(T_1,M.getGlobalVariable("parallel_region"),"",B->getTerminator());

  for(int i = 0 ; i < astdata.size() ; i++){
    ConstantInt *arraysize = ConstantInt::get(Type::getInt64Ty(B->getContext()), astdata[i].size());
    Constant* allocsize = ConstantExpr::getSizeOf(T_3);
    //allocsize = ConstantExpr::getTruncOrBitCast(allocsize, Type::getInt64Ty(B->getContext()));
    //ConstantInt* allocsize_new = ConstantInt::get(Type::getInt64Ty(B->getContext()), M.getDataLayout().getTypeAllocSize(T_3));
    Instruction *malloc_sub_region = CallInst::CreateMalloc(B->getTerminator(), Type::getInt64Ty(B->getContext()), T_2, allocsize, arraysize, nullptr, "malloced");
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

    for(int j = 0 ; j < astdata[i].size() ; j++){
      //errs()<<"ast value is "<<astdata[i][j]<<" "<<i<<" "<<j<<"\n";
      std::vector<llvm::Value*> indices;
      indices.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),i, false));
      GetElementPtrInst *gepinst_2 = GetElementPtrInst::Create(T_2,load_para_region_1,indices,"",B->getTerminator());
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
      new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, astdata[i][j], true)),gepinst_4,B->getTerminator());
    }
  }
}

void setAstData(Module &M, Function &F, LLVMContext &CTX, int ctr, int parallel_region_id){

  std::vector<int> temp;
  MDNode* ttex_array = F.getMetadata("ttex_array");

  if(ttex_array){
      //errs()<<"ttex array available"<<"\n";
    Value* v = dyn_cast<ValueAsMetadata> (ttex_array->getOperand(0))->getValue();
    if(v){
      //errs()<<"value received from ttex array"<<"\n";
      // errs()<<"Retreiving num elements for each outlined 2"<<n<<"\n";
      ConstantDataArray* init = dyn_cast<ConstantDataArray> (v);
      if(init){
        //errs()<<"array value of ttex array received"<<"\n";
        int n = init->getNumElements();
        for(unsigned i = 0 ; i < n ; i++){
          temp.push_back(init->getElementAsInteger(i));
        }
        //errs()<<"Retreiving num elements for each outlined "<<temp.size()<<"\n";
      }
    }
  }

  astdata[parallel_region_id] = temp;
  sizes[parallel_region_id] = temp.size();
}

void updateWorkId(Module &M, Function &F, LLVMContext &CTX){
  int ctr = 1;

  MDNode* parallel_id = F.getMetadata("parallel_id");

  Value* parallel_id_temp = dyn_cast<ValueAsMetadata>(parallel_id->getOperand(0))->getValue();
  auto* ci = dyn_cast<ConstantInt>(parallel_id_temp);

  int parallel_region_id = ci->getZExtValue()-1;

  for (Function::iterator block_iter = F.begin(), block_iter_end = F.end(); block_iter != block_iter_end; ++block_iter) {
    BasicBlock &B = *block_iter;
    for(BasicBlock::iterator instr_iter = B.begin(), instr_iter_end = B.end(); instr_iter != instr_iter_end; ++instr_iter){
      Instruction &I = *instr_iter;
      if(CallInst* call_inst = dyn_cast<CallInst>(&I)){
        Function* fn = call_inst->getCalledFunction();
        if(fn->getName() == "__kmpc_for_static_init_4"){
          llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
          call_inst->setOperand(9,itr_ci);
          setAstData(M,F,CTX,ctr,parallel_region_id);
          ctr++;
        }

        else if(fn->getName() == "__kmpc_single"){
          llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
          call_inst->setOperand(2,itr_ci);
          setAstData(M,F,CTX,ctr,parallel_region_id);
          ctr++;
        }
      }
    }
  }

  if(maxvuln_set){ // for all loops in outlined function split
    DominatorTree DT = llvm::DominatorTree();
    DT.recalculate(F);
    LoopInfoBase<BasicBlock, Loop>* LInfo = new llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>();
    LInfo->releaseMemory();
    LInfo->analyze(DT);

    bool loop_split_flag;

    for(LoopInfoBase<BasicBlock, Loop>::iterator loop_iter = LInfo->begin(), loop_iter_end = LInfo->end(); loop_iter != loop_iter_end; ++loop_iter){
      // testing

      loop_split_flag = true;

      Loop *ltemp = *loop_iter;
      BasicBlock *header = ltemp->getHeader();
      BasicBlock *Preheader = ltemp->getLoopPreheader();
      BasicBlock *LatchBlock = ltemp->getLoopLatch();

      std::vector<BasicBlock *> OriginalLoopBlocks = ltemp->getBlocks();
      std::vector<BasicBlock *> newLoopBlocks;

      for(int i = 0 ; i < OriginalLoopBlocks.size() ; i++){
        // OriginalLoopBlocks[i]->printAsOperand(errs(),false);
        std::string temp;
        raw_string_ostream temp_stream(temp);
        OriginalLoopBlocks[i]->printAsOperand(temp_stream,false);
        temp_stream.flush();
        if(OriginalLoopBlocks[i] != header && OriginalLoopBlocks[i] != LatchBlock){
          std::cout<<"block added to new loop blocks "<<temp<<std::endl; 
          newLoopBlocks.push_back(OriginalLoopBlocks[i]); // got blocks for new loop
        }
      }

      for(BasicBlock::iterator instr_iter = Preheader->begin(), instr_iter_end = Preheader->end(); instr_iter != instr_iter_end; ++instr_iter){
        Instruction &I = *instr_iter;
        if(CallInst* call_inst = dyn_cast<CallInst>(&I)){
          Function* fn = call_inst->getCalledFunction();
          if(fn->getName() == "__kmpc_for_static_init_4"){
            for(BasicBlock::iterator instr_iter_2 = newLoopBlocks[0]->begin(), instr_iter_end_2 = newLoopBlocks[0]->end(); instr_iter_2 != instr_iter_end_2; ++instr_iter_2){
              Instruction &I_2 = *instr_iter_2;
              if(SwitchInst* switch_inst = dyn_cast<SwitchInst>(&I_2)){
                loop_split_flag = false;
                break;
              } 
            }
            
            break;
          }
        }
      }

      if(loop_split_flag){
        
        errs()<<"!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!loop found!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<"\n\n";

        std::string ph_label, h_label, l_label;
        raw_string_ostream stream1(ph_label), stream2(h_label), stream4(l_label);
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
      

        if(LatchBlock){
          errs()<<"LatchBlock found"<<"\n";
          LatchBlock->printAsOperand(stream4,false);
          errs()<<l_label<<"\n";
        }

        errs()<<"\n\n\n\n\n";

        if(BranchInst *binst = dyn_cast<BranchInst>(header->getTerminator())){
          if(BasicBlock *compare_block = dyn_cast<BasicBlock> (binst->getOperand(1))){
            LoopSplit(ltemp, 2, compare_block, parallel_region_id, OriginalLoopBlocks, newLoopBlocks); // 2 is split factor which will be different for different parallel region
          }
        }
      }
        
    }
  }
}

namespace {
  // Hello2 - The second implementation with getAnalysisUsage implemented.
  struct TtexPass : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    TtexPass() : ModulePass(ID) {
      initializeTtexPassPass(*PassRegistry::getPassRegistry());
    }

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

      std::vector<int> temp;
      std::vector<int> sizes_temp(counter,0);
      std::vector< std::vector<int> >astdata_temp(counter,temp);

      sizes = sizes_temp;
      astdata = astdata_temp;


      for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
        Function &F = *func_iter;

        if (!F.isDeclaration()) {
           //errs()<<"Function name is:"<<F.getName()<<"\n";
          if(F.getName().contains(".omp_outlined.")){
            //errs()<<"omp outlined function called\n";
            errs()<<"Function name is:"<<F.getName()<<"\n";
            updateWorkId(M,F,CTX);
          }
        }
      }

      for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
        Function &F = *func_iter;

        if (!F.isDeclaration()) {
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

char TtexPass::ID = 0;
static RegisterPass<TtexPass> C("ttex", "execute all pass operations");


INITIALIZE_PASS_BEGIN(TtexPass, "ttexpass", 
                      "Some description for the Pass", 
                      false, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass) // Or whatever your Pass dependencies
INITIALIZE_PASS_END(TtexPass, "ttexpass",
                    "Some description for the Pass", 
                    false, false)

ModulePass* llvm::createTtexPass() {
  return new TtexPass();
}

// added to the cmakescalar list as well else will get linker error