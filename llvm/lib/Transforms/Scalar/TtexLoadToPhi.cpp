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

void findCorrespondingInductionVar(Loop *L, Instruction *I, BasicBlock * B){
  
}

bool LoopSplit(Loop *L, unsigned count, BasicBlock *ExitBlock, Instruction* ind_inst, Instruction* upper_inst, Instruction *cmp_inst, bool upper_bound_phi, bool lower_bound_phi){
  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *LatchBlock = L->getLoopLatch();
  //BasicBlock *ExitingBlock = L->getExitingBlock();
  //BasicBlock *ExitBlock;

  llvm::Value* temp_v = cmp_inst;
  llvm::Value* originalInd;
  llvm::Value* upperBound;

  errs()<<"---- inside loop split -----\n";

  if(count == 0){
    return false;
  }

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

  /*assigning iteration range*/

  // llvm::ConstantInt *start_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),0, false); // starting value
  llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),1, false); // iteration value
  llvm::ConstantInt *end_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),count-1, false);
  llvm::ConstantInt *end_ci_outer = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),count, false);


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

  llvm::PHINode* phi_ind_node;
  llvm::PHINode* phi_upper_node;

  if(!lower_bound_phi){
    phi_ind_node = prehead.CreatePHI(ind_inst->getType(), 0, "indvar");
    phi_ind_node->addIncoming(ind_inst, Header);
    phi_ind_node->addIncoming(ind_inst, InnerLoopHeader);
    if(!upper_bound_phi) {
      phi_upper_node = prehead.CreatePHI(upper_inst->getType(), 0 , "upper");
      phi_upper_node->addIncoming(upper_inst, Header);
    }

    else {
      phi_upper_node = dyn_cast<PHINode>(upper_inst);
    }
    errs()<<"original ind is a memory\n";
    //load_original = prehead.CreateLoad(llvm::IntegerType::getInt32Ty(CTX), originalInd, "check");
    errs()<<"------------- this works 2------------\n";
    //llvm::AllocaInst* allocate_start = prehead.CreateAlloca(llvm::IntegerType::getInt32Ty(CTX), nullptr ,"iterator");
    allocate_start = prehead.CreateAlloca(llvm::IntegerType::getInt32Ty(CTX), nullptr ,"iterator");
    allocate_end = prehead.CreateAlloca(llvm::IntegerType::getInt32Ty(CTX),nullptr, "iterator_bound");
    errs()<<"--- alloca works---\n";
    llvm::StoreInst* store_start = prehead.CreateStore(phi_ind_node,allocate_start,false);
    // llvm::LoadInst* load_start =  prehead.CreateLoad(llvm::IntegerType::getInt64Ty(CTX), allocate_start, "");
    errs()<<"---- store works----\n";
    llvm::Value* ind_end = prehead.CreateNSWAdd(end_ci, phi_ind_node,"");
    errs()<<"------ create NSWADD works----\n";
    std::string str10;
    raw_string_ostream stream10(str10);
    ind_end->getType()->print(stream10,false);
    errs()<<"ind end type is "<<str10<<"\n";
    prehead.CreateStore(ind_end, allocate_end, false);
    errs()<<"this is the end \n";
  }

  else {

    phi_ind_node = dyn_cast<PHINode>(ind_inst);

    if(!upper_bound_phi) {
      phi_upper_node = prehead.CreatePHI(upper_inst->getType(), 0 , "upper");
      phi_upper_node->addIncoming(upper_inst, Header);
      phi_upper_node->addIncoming(ind_inst, InnerLoopHeader);
    }

    else {
      phi_upper_node = dyn_cast<PHINode>(upper_inst);
    }

    allocate_start = prehead.CreateAlloca(llvm::IntegerType::getInt32Ty(CTX), nullptr ,"iterator");
    allocate_end = prehead.CreateAlloca(llvm::IntegerType::getInt32Ty(CTX),nullptr, "iterator_bound");
    errs()<<"--- alloca works---\n";
    llvm::StoreInst* store_start = prehead.CreateStore(phi_ind_node,allocate_start,false);
    // llvm::LoadInst* load_start =  prehead.CreateLoad(llvm::IntegerType::getInt64Ty(CTX), allocate_start, "");
    errs()<<"---- store works----\n";
    llvm::Value* ind_end = prehead.CreateNSWAdd(end_ci, phi_ind_node,"");
    errs()<<"------ create NSWADD works----\n";
    std::string str10;
    raw_string_ostream stream10(str10);
    ind_end->getType()->print(stream10,false);
    errs()<<"ind end type is "<<str10<<"\n";
    prehead.CreateStore(ind_end, allocate_end, false);
    errs()<<"this is the end \n";
  }

  Value* boolValue = ConstantInt::get(Type::getInt1Ty(CTX), 1);
  //Value* cmpvalue = prehead.CreateICmp
  //prehead.CreateBr(InnerLoopHeader);
  prehead.CreateCondBr(boolValue, InnerLoopHeader, InnerLoopPreheader);

  errs()<<"------------- this works------------\n";

  IRBuilder<> head(InnerLoopHeader);

  llvm::Value* compare;
  llvm::LoadInst* load_ind = head.CreateLoad(llvm::IntegerType::getInt32Ty(CTX),allocate_start,"inner_itr_start");
  llvm::LoadInst* load_ind_end = head.CreateLoad(llvm::IntegerType::getInt32Ty(CTX),allocate_end,"inner_itr_end");
  // inner loop iterator is new so has to be loaded irrespective of outer loop iterator
  //llvm::Value* ind_end = prehead.CreateNSWAdd(end_ci, originalInd,"");
  compare = head.CreateICmpSLE(load_ind,load_ind_end);
  head.CreateCondBr(compare, TempCompare, LatchBlock); // if true execute the original body of loop else move to latch of original
  //newLoopBlocks[0],

  IRBuilder<> tempblock(TempCompare);
  llvm::LoadInst* load_outer_bound;
  llvm::Value* check;
  llvm::LoadInst* load_ind_start;

  
  load_ind_start = tempblock.CreateLoad(llvm::IntegerType::getInt32Ty(CTX),allocate_start,"inner_itr_end");

  std::string str12;
  raw_string_ostream stream12(str12);
  phi_upper_node->getType()->print(stream12,false);
  errs()<<"upper bound type is "<<str12<<"\n";

  // load_outer_bound = tempblock.CreateLoad(llvm::IntegerType::getInt64Ty(CTX), upperBound,"");
  check = tempblock.CreateICmpSLE(load_ind_start,phi_upper_node,"");

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

void getInductionVariableUsingCmp(Loop *L){
  BasicBlock *header = L->getHeader();
  BasicBlock *latch = L->getLoopLatch();

  SmallVector<Instruction*> ind_var_list;
  SmallVector<Instruction*> upper_bound_list;

  if(BranchInst *BI = dyn_cast<BranchInst>(header->getTerminator())){
    if(BI->isConditional()){
      SmallVector<BasicBlock*> SuccsExiting;
      L->getExitingBlocks(SuccsExiting);
      SmallVector<BasicBlock*, 8> SuccsExit;
      L->getExitBlocks(SuccsExit);

      for(int i = 0 ; i < SuccsExiting.size(); i++){
         if(BI->getSuccessor(1) == SuccsExiting[i]){
          // found an icmp instruction
          Instruction *I = dyn_cast<ICmpInst>(BI->getCondition());
          errs() << "Found the latch compare instruction for the loop: "<<*I<<"\n";
          if(LoadInst *LI = dyn_cast<LoadInst>(I->getOperand(0))){
            errs() <<"Found the corresponding load instruction of the value "<<*LI<<"\n";
            if(LI->getParent() == header){
              //ind_var_list.push_back(LI);
              if(LoadInst *LI_2 = dyn_cast<LoadInst>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExiting[i],LI,LI_2,I,false,false);
              }

              else if(PHINode *PN = dyn_cast<PHINode>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExiting[i],LI,PN,I,false,true);
              }
            }
          }

          else if(PHINode *PN = dyn_cast<PHINode>(I->getOperand(0))){
            errs() <<"Found the corresponding PHI node of the value "<<*PN<<"\n";
            if(PN->getParent() == header){
              ind_var_list.push_back(PN);
              if(LoadInst *LI_2 = dyn_cast<LoadInst>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExiting[i],PN,LI_2,I,true,false);
              }

              else if(PHINode *PN_2 = dyn_cast<PHINode>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExiting[i],PN,PN_2,I,true,true);
              }
            }
          }

          return;
         }
      }

      for(int i = 0 ; i < SuccsExit.size(); i++){
         if(BI->getSuccessor(1) == SuccsExit[i]){
          // found an icmp instruction
          Instruction *I = dyn_cast<ICmpInst>(BI->getCondition());
          errs() << "Found the latch compare instruction for the loop: "<<*I<<"\n";
          if(LoadInst *LI = dyn_cast<LoadInst>(I->getOperand(0))){
            errs() <<"Found the corresponding load instruction of the value "<<*LI<<"\n";
            if(LI->getParent() == header){
              //ind_var_list.push_back(LI);
              if(LoadInst *LI_2 = dyn_cast<LoadInst>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExit[i],LI,LI_2,I,false,false);
              }

              else if(PHINode *PN = dyn_cast<PHINode>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExit[i],LI,PN,I,false,true);
              }
            }
          }

          else if(PHINode *PN = dyn_cast<PHINode>(I->getOperand(0))){
            errs() <<"Found the corresponding PHI node of the value "<<*PN<<"\n";
            if(PN->getParent() == header){
              ind_var_list.push_back(PN);
              if(LoadInst *LI_2 = dyn_cast<LoadInst>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExit[i],PN,LI_2,I,true,false);
              }

              else if(PHINode *PN_2 = dyn_cast<PHINode>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExit[i],PN,PN_2,I,true,true);
              }
            }
          }
          return;
         }
      }
    }

    else {
        return;
    }  
  }

  else if(BranchInst *BI = dyn_cast<BranchInst>(latch->getTerminator())){
    if(BI->isConditional()){
      SmallVector<BasicBlock*> SuccsExiting;
      L->getExitingBlocks(SuccsExiting);
      SmallVector<BasicBlock*, 8> SuccsExit;
      L->getExitBlocks(SuccsExit);

      for(int i = 0 ; i < SuccsExiting.size(); i++){
         if(BI->getSuccessor(1) == SuccsExiting[i]){
          //found an icmp instruction
          Instruction *I = dyn_cast<ICmpInst>(BI->getCondition());
          errs() << "Found the latch compare instruction for the loop: "<<*I<<"\n";
          if(LoadInst *LI = dyn_cast<LoadInst>(I->getOperand(0))){
            errs() <<"Found the corresponding load instruction of the value "<<*LI<<"\n";
            if(LI->getParent() == header){
              //ind_var_list.push_back(LI);
              if(LoadInst *LI_2 = dyn_cast<LoadInst>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExiting[i],LI,LI_2,I,false,false);
              }

              else if(PHINode *PN = dyn_cast<PHINode>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExiting[i],LI,PN,I,false,true);
              }
            }
          }

          else if(PHINode *PN = dyn_cast<PHINode>(I->getOperand(0))){
            errs() <<"Found the corresponding PHI node of the value "<<*PN<<"\n";
            if(PN->getParent() == header){
              ind_var_list.push_back(PN);
              if(LoadInst *LI_2 = dyn_cast<LoadInst>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExiting[i],PN,LI_2,I,false,false);
              }

              else if(PHINode *PN_2 = dyn_cast<PHINode>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExiting[i],PN,PN_2,I,false,true);
              }
            }
          }
          return;
         }
      }

      for(int i = 0 ; i < SuccsExit.size(); i++){
         if(BI->getSuccessor(1) == SuccsExit[i]){
          // found an icmp instruction
          Instruction *I = dyn_cast<ICmpInst>(BI->getCondition());
          errs() << "Found the latch compare instruction for the loop: "<<*I<<"\n";
          if(LoadInst *LI = dyn_cast<LoadInst>(I->getOperand(0))){
            errs() <<"Found the corresponding load instruction of the value "<<*LI<<"\n";
            if(LI->getParent() == header){
              //ind_var_list.push_back(LI);
              if(LoadInst *LI_2 = dyn_cast<LoadInst>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExit[i],LI,LI_2,I,false,false);
              }

              else if(PHINode *PN = dyn_cast<PHINode>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExit[i],LI,PN,I,false,true);
              }
            }
          }

          else if(PHINode *PN = dyn_cast<PHINode>(I->getOperand(0))){
            errs() <<"Found the corresponding PHI node of the value "<<*PN<<"\n";
            if(PN->getParent() == header){
              ind_var_list.push_back(PN);
              if(LoadInst *LI_2 = dyn_cast<LoadInst>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExit[i],PN,LI_2,I,false,false);
              }

              else if(PHINode *PN_2 = dyn_cast<PHINode>(I->getOperand(1))){
                LoopSplit(L,2,SuccsExit[i],PN,PN_2,I,false,true);
              }
            }
          }
          return;
         }
      }
    }

    else {
        return;
    }  
  }

  return;
}

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

        if(F.getName().contains(".omp_outlined.") && F.getName() != ".omp_outlined._debug__"){
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
            // if(LIB->begin() == LIB->end()){
            //   errs()<<"no loop info\n";
            // }

            // // else {
            // //   errs()<<"\n";
            // // }

            // else {
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
                  // std::vector<Instruction *> I = getInductionVariable(L);

                  // for(int i = 0 ; i < I.size() ; i++){
                  //   errs()<<"----------- replace load with phi------------\n";
                  //   I[i]->replaceAllUsesWith(PHINode::Create(I[i]->getType(),0,"",I[i])); // create a phi node replacement here -- this would help reducing the issue with load and store
                  // }

                getInductionVariableUsingCmp(L); 

              }
            
              // PreservedAnalyses PA;
              // PA.preserve<DominatorTreeAnalysis>();
              // PA.preserve<LoopAnalysis>();
              // PA.preserve<ScalarEvolutionAnalysis>();
              // PA.preserve<DependenceAnalysis>();
              // if (MSSAAnalysis)
              //   PA.preserve<MemorySSAAnalysis>();
              // // BPI maps conditional terminators to probabilities, LoopSimplify can insert
              // // blocks, but it does so only by splitting existing blocks and edges. This
              // // results in the interesting property that all new terminators inserted are
              // // unconditional branches which do not appear in BPI. All deletions are
              // // handled via ValueHandle callbacks w/in BPI.
              // PA.preserve<BranchProbabilityAnalysis>();
              // return PA;

            //}
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