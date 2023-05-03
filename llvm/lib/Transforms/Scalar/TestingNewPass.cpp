#include "llvm/Transforms/Scalar/TestingNewPass.h"
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
#include "llvm/Analysis/LoopNestAnalysis.h"
#include "llvm/ADT/BreadthFirstIterator.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/Analysis/ValueTracking.h"
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
#include <optional>
#include "llvm/Transforms/Scalar/Ttex.h"
#include "llvm/IR/DataLayout.h"

using namespace llvm;

std::vector< std::vector< std::pair<int,int> > > astdata; //storing the sub region info -- but need to fix the id's to correct location
std::vector<int> sizes;
std::vector< std::vector<int> > loop_split;

bool maxvuln_set = true;

bool LoopSplit(Loop *L, unsigned count, BasicBlock *ExitBlock, int parallel_id, int sub_id){
  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *LatchBlock = L->getLoopLatch();
  // BasicBlock *ExitingBlock = L->getExitingBlock();
  //BasicBlock *ExitBlock;

  llvm::Value* temp_v;
  llvm::Value* originalInd;
  llvm::Value* upperBound;

  errs()<<"---- inside loop split -----\n";

  if(count == 0){
    return false;
  }

  bool correct_loop_format = false;

  llvm::BasicBlock::iterator indInst = Header->begin(); // this will always be loading start value

  std::string str1;
  raw_string_ostream stream1(str1);

  int loop_type;

  if(isa<llvm::LoadInst> (indInst)){// || isa<llvm::PHINode> (indInst)){
    // this should always be true
    loop_type = 1;
    // llvm::Instruction *temp_i = &*indInst;
    // originalInd = temp_i->getOperand(0);
    // std::cout<<"name of the induction variable is: "<<originalInd->getName().str()<<std::endl;
    // if(originalInd->getName().contains("sections")){
    //   std::cout<<"Sections looks like a loop but is not a loop"<<std::endl;
    //   return false;
    // }
  }

  else if(isa<llvm::PHINode> (indInst)){
    loop_type = 2;
  }

  else {
    std::cout<<"XXXXXXXXXXXXXXXXXXXXXx wrong loop format: (Header always loads or phi first) xXXXXXXXXXXXXXXXXXXXXXXXXXXXX"<<std::endl;
    return false;
  }

  llvm::BasicBlock *temp;

  if(loop_type == 1){
    temp = Header;
  }

  else {
    temp = LatchBlock;
  }

  llvm::BasicBlock::iterator I = temp->end();
  --I; // icmp is the second last instruction
  --I;

  bool originalIndTypeConst = false;
  bool upperBoundTypeConst = false;

  if(isa<llvm::ICmpInst> (I)){
    //errs()<<"----- retrieving upper and lower bound below------\n";
    temp_v = &*I; // iterator to instruction
    for(llvm::BinaryOperator::op_iterator oi = I->op_begin(), oi_end = I->op_end(); oi != oi_end; ++oi){
        std::cout<<"name of the operator:: "<<oi->get()->getName().str()<<std::endl;
    }

    errs()<<"----- retrieving upper and lower bound below ===------\n";
    upperBound = (&*I)->getOperand(1);
    originalInd = (&*I)->getOperand(0);

    if(isa<llvm::ConstantInt> (originalInd)){
      //do nothing
      errs()<<"---- it is a constant ------ \n";
      originalIndTypeConst = true;
    }

    else {
      // if not a constant then a load instruction
      errs()<<"original ind found\n";
      if(dyn_cast<llvm::LoadInst>(originalInd)){
        llvm::LoadInst* test_load = cast<llvm::LoadInst>(originalInd);
        originalInd = test_load->getOperand(0);
        errs()<<"-----------originalInd  executed here 1--------\n";
      }

      else if(dyn_cast<llvm::SExtInst>(originalInd)){
        llvm::SExtInst* s = cast<llvm::SExtInst>(originalInd);
        llvm::Value* test = s->getOperand(0);
        if(dyn_cast<llvm::LoadInst>(test)){
          llvm::LoadInst* test_load = cast<llvm::LoadInst>(test);
          originalInd = test_load->getOperand(0);
          errs()<<"-----------originalInd  executed here--------\n";
        }
      }

      else {
        errs()<<"--- this is a variable -------\n";
        originalIndTypeConst = true;
        // originalInd = &originalInd; need to get a value of i32* for i64 here 
      }
    }

    errs()<<"upper bound retrieval\n";

    if(isa <llvm::ConstantInt> (upperBound)){
      //do nothing
      upperBoundTypeConst = true;
    }

    else {
      // if not a constant then a load instruction
      if(dyn_cast<llvm::LoadInst>(upperBound)){
        llvm::LoadInst* test_load = cast<llvm::LoadInst>(upperBound);
        upperBound = test_load->getOperand(0);
      }

      else if(dyn_cast<llvm::SExtInst>(upperBound)){
        llvm::SExtInst* s = cast<llvm::SExtInst>(upperBound);
        llvm::Value* test = s->getOperand(0);
        if(dyn_cast<llvm::LoadInst>(test)){
          llvm::LoadInst* test_load = cast<llvm::LoadInst>(test);
          upperBound = test_load->getOperand(0);
          errs()<<"-----------upper bound executed here--------\n";
        }
      }

      else {
        errs()<<"--- this is a variable -------\n";
        upperBoundTypeConst = true;
      }
    }
  }

  else {
    errs()<<"---- no icmp -------\n";
    return false;
  }

  // for(llvm::BasicBlock::iterator I = temp->begin(), Iend = temp->end(); I != Iend ; ++I){
  //   if (isa <llvm::ICmpInst> (I)){
  //     correct_loop_format = true;
  //     std::cout<<"found compare instruction"<<std::endl;
  //     temp_v = &*I; // iterator to instruction
  //     for(llvm::BinaryOperator::op_iterator oi = I->op_begin(), oi_end = I->op_end(); oi != oi_end; ++oi){
  //         std::cout<<"name of the operator:: "<<oi->get()->getName().str()<<std::endl;
  //     }
  //     upperBound = (&*I)->getOperand(1);
  //     //originalInd = (&*I)->getOperand(0);
  //     // if(isa <llvm::ConstantInt> (upperBound)){
  //     //   //do nothing
  //     // }

  //     // else {
  //     //   // if not a constant then a load instruction
  //     //   llvm::LoadInst* test_load = cast<llvm::LoadInst>(upperBound);
  //     //   upperBound = test_load->getOperand(0);
  //     // }

  //     upperBound->print(stream1,false);
  //     std::cout<<"Name of the upper bound variable is: "<<str1<<std::endl;  
  //   }

  //   std::cout<<"Loop execution in progress"<<std::endl;
  // }

  // if(!correct_loop_format){
  //   std::cout<<"XXXXXXXXXXXXXXXXXXXXXx wrong loop format: (No icmp compare) xXXXXXXXXXXXXXXXXXXXXXXXXXXXX"<<std::endl;
  //   return false;
  // }

  


  errs()<<"-------------------------- loop split executed -------------------------------\n";

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
  llvm::BasicBlock* TempCompare = BasicBlock::Create(CTX,"CheckOverShoot",Func);

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

  std::cout<<"Loop completed for identifying upperbound"<<std::endl;

  /*Header block*/

  // load and store induction variable values
  std::string str2;
  raw_string_ostream stream2(str2);
  originalInd->getType()->print(stream2,false);

  if(originalInd->getType()->isIntegerTy()){
    errs()<<"--- is an integer ---\n";
  }

  else {
    errs()<<"-------- not and integer -------\n";
  }

  if(originalInd->getType()->isPointerTy()){
    errs()<<"---- is a pointer ---- \n";
  }

  else {
    errs()<<" ------ not a pointer -----\n";
  }

  errs()<<"------------type of original induction is ================:"<<str2<<"\n";
  errs()<<"------------- this works------------\n";
  IRBuilder<> prehead(InnerLoopPreheader);
  errs()<<"------------- this works 1------------\n";
  
  llvm::AllocaInst* allocate_start;
  llvm::AllocaInst* allocate_end;
  llvm::LoadInst* load_original;

  // if(originalIndTypeConst){
  //   errs()<<"inside this if\n";
  //   llvm::Value *intptroriginal = prehead.CreateIntToPtr(originalInd, llvm::Type::getInt32PtrTy(CTX),"");
  //   load_original = prehead.CreateLoad(llvm::Type::getInt32Ty(CTX), intptroriginal, "check");
  //   std::string str10;
  //   raw_string_ostream stream10(str10);
  //   load_original->getType()->print(stream10,false);
  //   errs()<<"in to ptr worked type "<<str10<<"\n";
  // }

  // else {
    
  // }


  if(!originalIndTypeConst){
    errs()<<"original ind is a memory\n";
    load_original = prehead.CreateLoad(llvm::IntegerType::getInt32Ty(CTX), originalInd, "check");
    errs()<<"------------- this works 2------------\n";
    //llvm::AllocaInst* allocate_start = prehead.CreateAlloca(llvm::IntegerType::getInt32Ty(CTX), nullptr ,"iterator");
    allocate_start = prehead.CreateAlloca(llvm::IntegerType::getInt32Ty(CTX), nullptr ,"iterator");
    allocate_end = prehead.CreateAlloca(llvm::IntegerType::getInt32Ty(CTX),nullptr, "iterator_bound");
    errs()<<"--- alloca works---\n";
    llvm::StoreInst* store_start = prehead.CreateStore(load_original,allocate_start,false);
    // llvm::LoadInst* load_start =  prehead.CreateLoad(llvm::IntegerType::getInt64Ty(CTX), allocate_start, "");
    errs()<<"---- store works----\n";
    llvm::Value* ind_end = prehead.CreateNSWAdd(end_ci, load_original,"");
    errs()<<"------ create NSWADD works----\n";
    std::string str10;
    raw_string_ostream stream10(str10);
    ind_end->getType()->print(stream10,false);
    errs()<<"ind end type is "<<str10<<"\n";
    prehead.CreateStore(ind_end, allocate_end, false);
    errs()<<"this is the end \n";
  }

  else {
    errs()<<"------- inside this------\n";
    //llvm::Value *intptroriginal = prehead.CreateIntToPtr(originalInd, llvm::Type::getInt32PtrTy(CTX),"");
    //load_original = prehead.CreateLoad(llvm::Type::getInt32Ty(CTX), intptroriginal, "check");
    allocate_start = prehead.CreateAlloca(llvm::IntegerType::getInt64Ty(CTX), nullptr ,"iterator");
    //ConstantInt *testval = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),3, false);
    prehead.CreateStore(originalInd,allocate_start,false);
  }

  prehead.CreateBr(InnerLoopHeader);

  errs()<<"------------- this works------------\n";

  IRBuilder<> head(InnerLoopHeader);

  llvm::Value* compare;
  llvm::LoadInst* load_ind = head.CreateLoad(llvm::IntegerType::getInt64Ty(CTX),allocate_start,"inner_itr_start");
  // inner loop iterator is new so has to be loaded irrespective of outer loop iterator

  if(!originalIndTypeConst){
    llvm::LoadInst* load_ind_end = head.CreateLoad(llvm::IntegerType::getInt64Ty(CTX),allocate_end,"inner_itr_end");
    // llvm::LoadInst* load_old_ind = head.CreateLoad(llvm::IntegerType::getInt32Ty(CTX), originalInd, "outer_itr");
    // llvm::BinaryOperator* new_ind = head.CreateNSWAdd(load_start,load_old_ind,"induction_value");
    compare = head.CreateICmpSLE(load_ind,load_ind_end);
  }

  else {
    errs()<<"------ nsw add is an issue ------\n";
    llvm::Value* ind_end = prehead.CreateNSWAdd(end_ci, originalInd,"");
    errs()<<"--------- still an issue ------------\n";
    compare = head.CreateICmpSLE(originalInd, ind_end);
  }
  
  head.CreateCondBr(compare, TempCompare, LatchBlock); // if true execute the original body of loop else move to latch of original
  //newLoopBlocks[0],

  IRBuilder<> tempblock(TempCompare);
  llvm::LoadInst* load_outer_bound;
  llvm::Value* check;
  llvm::LoadInst* load_ind_start;

  load_ind_start = tempblock.CreateLoad(llvm::IntegerType::getInt64Ty(CTX),allocate_start,"inner_itr_end");

  std::string str12;
  raw_string_ostream stream12(str12);
  upperBound->getType()->print(stream12,false);
  errs()<<"upper bound type is "<<str12<<"\n";

  if(!upperBoundTypeConst){
    // llvm::Value *intptrupperbound = prehead.CreateIntToPtr(upperBound, llvm::Type::getInt32PtrTy(CTX),"");
    // load_outer_bound = tempblock.CreateLoad(llvm::IntegerType::getInt32Ty(CTX), intptrupperbound,"");
    load_outer_bound = tempblock.CreateLoad(llvm::IntegerType::getInt64Ty(CTX), upperBound,"");
    check = tempblock.CreateICmpSLE(load_ind_start,load_outer_bound,"");
  }

  else {
    check = tempblock.CreateICmpSLE(originalInd,upperBound,"");
  }

  // else {
  //   load_outer_bound = tempblock.CreateLoad(llvm::IntegerType::getInt32Ty(CTX), upperBound,"");
  // }

  tempblock.CreateCondBr(check, newLoopBlocks[0], LatchBlock);
  errs()<<"--------- still an issue 2 ------------\n";

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

  ///// Uncomment below to insert ompt_test 

//   FunctionType *testing = FunctionType::get(
//       Type::getVoidTy(CTX),
//       {IntegerType::getInt32Ty(CTX), IntegerType::getInt32Ty(CTX)},
//       //PointerType::getPointerAddressSpace(),
//       /*IsVarArgs=*/false);

//   FunctionCallee hookTest = M->getOrInsertFunction("ompt_test", testing);
//   //Function *hook = dyn_cast<Function>(hookTest.getCallee());
//   std::vector<Value*> args;
//   ConstantInt *arg1 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),parallel_id, false);
//   ConstantInt *arg2 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),sub_id, false);
//   //ConstantInt *arg3 = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),-1, false);
//   args.push_back(arg1);
//   args.push_back(arg2);
//  // args.push_back(arg3);
//   llvm::CallInst::Create(hookTest,args,"",&*(--LatchBlock->end()));

//// ompt test added above

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
  return true;
}

std::vector<int> protectFor(Module &M, Function &F, LLVMContext &CTX, int parallel_region_id, ModuleAnalysisManager &AM){
  
  std::vector<int> temp;

  auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

  if(!AM.empty()){
    errs()<<"---- checking if we get some result in the analysis manager -------\n";
    //errs()<<FAM.getResult<LoopAnalysis>(F);
  }
  //auto &LAM = FAM.getResult<LoopAnalysisManagerFunctionProxy>(F).getManager();

  auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
  //LoopInfo *LInfo = &FAM.getResult<LoopAnalysis>(F);
  auto &TTI = FAM.getResult<TargetIRAnalysis>(F);
  DominatorTree* DT = &FAM.getResult<DominatorTreeAnalysis>(F);
  auto &AC = FAM.getResult<AssumptionAnalysis>(F);
  auto &ORE = FAM.getResult<OptimizationRemarkEmitterAnalysis>(F);
  
  // DominatorTree DT = llvm::DominatorTree();
  DT->recalculate(F);
  LoopInfoBase<BasicBlock, Loop>* LInfo = new llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>();
  LInfo->releaseMemory();
  LInfo->analyze(*DT);

  errs()<<"checking for errors"<<"\n";

  if(LInfo->begin() == LInfo->end()){
    errs()<<"no loop info\n";
  }

  int ctr = 0;

  for(LoopInfo::iterator loop_iter = LInfo->begin(), loop_iter_end = LInfo->end(); loop_iter != loop_iter_end; ++loop_iter){ 
   //for (auto *ltemp : *LInfo){
    errs()<<"parsing through the loop\n";
    //testing
    Loop *ltemp = *loop_iter;

    for (BasicBlock *BB : ltemp->getBlocks()) {
      for (Instruction &I : *BB) {
        if (auto *PHI = dyn_cast<PHINode>(&I)) {
          if (SE.isSCEVable(PHI->getType())) {
            const SCEV *S = SE.getSCEV(PHI);
            if (auto *AddRec = dyn_cast<SCEVAddRecExpr>(S)) {
              const SCEV *Start = AddRec->getStart();
              const SCEV *Step = AddRec->getStepRecurrence(SE);
              //const SCEV *UpperBound = AddRec->getMaxExpr(SE);
              errs() << "Loop with induction variable " << PHI->getName() << "\n";
              errs() << "Start: " << *Start << "\n";
              errs() << "Step: " << *Step << "\n";
              //errs() << "Upper Bound: " << *UpperBound << "\n";
            }
          }
        }
      }
    }

    //Loop::LoopBounds lbound = ltemp->getBounds(SE);
    auto lbound = ltemp->getBounds(SE);
    PHINode *linduction = ltemp->getInductionVariable(SE);
    if(linduction == nullptr){
      errs()<<"################### the given induction returned null ptr ###############\n";
    }

    if(ltemp->isLCSSAForm(*DT)){
      errs()<<" --------------------------- ((((( loop is in lcssa form ))))) --------------------\n";
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

    // std::string h_label;
    // raw_string_ostream stream2(h_label);

    // if(header){
    //   errs()<<"header found"<<"\n";
    //   header->printAsOperand(stream2,false);
    //   errs()<<h_label<<"\n";
    // }

    errs()<<"Found a loop"<<"\n";

    bool split_check = false;

    if(BranchInst *binst = dyn_cast<BranchInst>(header->getTerminator())){
      if(BasicBlock *compare_block = dyn_cast<BasicBlock> (binst->getOperand(1))){ // exiting block of the loop
        //LoopSplit(ltemp, loop_split[parallel_region_id][ctr], compare_block, parallel_region_id, ctr);
        //split_check = LoopSplit(ltemp, 100, compare_block, parallel_region_id, ctr); // runs ompt_test as many times as the iteration (a lot)
      }
    }

    if(split_check){
      ctr++;
      temp.push_back(1); // push_back value from data analyzer and feeder for new loop split values
    }
      
   // errs()<<"Counter value of the number of loops --------------------------"<<ctr<<"\n";
  }

  return temp;
}

int setAstData(Module &M, Function &F, LLVMContext &CTX, int ctr, int pid, BasicBlock &callblock){

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

  // astdata.push_back(temp);
  // sizes.push_back(temp.size());

  //errs()<<"------------ parallel id value is------"<<ci->getZExtValue()<<"\n";
  //errs()<<"astdata size and sizes size: "<<astdata.size()<<" "<<sizes.size()<<"\n";

  astdata[pid] = temp;
  sizes[pid] = temp.size();

  // loop_split.push_back();

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

void updateWorkId(Module &M, Function &F, LLVMContext &CTX, ModuleAnalysisManager &AM){
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
        call_inst->print(dumpdata,false);
        //errs()<<"call instruction is "<<dumptest<<"\n";
        if(fn){
            if(fn->getName() == "__kmpc_for_static_init_4"){
                llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
                call_inst->setOperand(9,itr_ci);
                setAstData(M,F,CTX,ctr, p_id,B);
                ctr++;
            }

            else if(fn->getName() == "__kmpc_single"){
                llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
                call_inst->setOperand(2,itr_ci);
                setAstData(M,F,CTX,ctr, p_id,B);
                ctr++;
            }
        }
      }
    }
  }

  /*loop split code*/

  if(maxvuln_set){
    loop_split[p_id] = protectFor(M,F,CTX,p_id,AM);
  }

  /*end of loop split code*/

}

PreservedAnalyses TestingNewPass::run(Module &M,
                                      ModuleAnalysisManager &AM) {

  LLVMContext &CTX = M.getContext();
  int counter = 0;



  errs()<<"#################Executing the pass#######################"<<"\n";

  for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
    Function &F = *func_iter;

    errs()<<"Function name is:"<<F.getName()<<"\n";

    if (!F.isDeclaration()) {
        //errs()<<"Function name is:"<<F.getName()<<"\n";
      if(F.getName().contains(".omp_outlined.") && !F.getName().contains("debug")){
        counter++;
      }
    }
  }

  std::vector< std::pair<int,int> > temp;
  std::vector<int> loop_split_temp;
  std::vector< std::vector<int> > temp_loop_split(counter,loop_split_temp);
  std::vector<int> sizes_temp(counter,0);
  std::vector< std::vector< std::pair<int,int> > >astdata_temp(counter,temp);

  sizes = sizes_temp;
  astdata = astdata_temp;
  loop_split = temp_loop_split;

  for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
    Function &F = *func_iter;
  
    if (!F.isDeclaration()) {
      if(F.getName().contains(".omp_outlined.") && !F.getName().contains("debug")){
        errs()<<"Function Name New Pass: "<< F.getName() << "\n";
        updateWorkId(M,F,CTX,AM);
      }
    }
  }

  return PreservedAnalyses::all();
}

// test module pass with loop simplify -- check if loop information is available before module pass