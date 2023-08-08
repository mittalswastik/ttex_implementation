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

bool LoopSplit(Loop *L, unsigned count, BasicBlock *ExitBlock, Instruction* ind_inst, Value* upper_inst, Instruction *cmp_inst, bool upper_bound_phi, bool lower_bound_phi){
  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *LatchBlock = L->getLoopLatch();
  //BasicBlock *ExitingBlock = L->getExitingBlock();
  //BasicBlock *ExitBlock;

  if(!ExitBlock){
    errs() <<"---- no exit block found ----\n";
  }

  llvm::Value* temp_v = cmp_inst;
  llvm::Value* originalInd;
  llvm::Value* upperBound;

  errs()<<"---- inside loop split -----\n";

  std::string str;
  raw_string_ostream stream_1(str);
  ind_inst->getType()->print(stream_1,false);
  errs()<<"in to ptr worked type "<<str<<"\n";

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
  llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(CTX),1, false); // iteration value
  llvm::ConstantInt *end_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(CTX),count-1, false);
  llvm::ConstantInt *end_ci_outer = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(CTX),count, false);


  errs()<<"------------- this works------------\n";
  IRBuilder<> prehead(InnerLoopPreheader);
  errs()<<"------------- this works 1------------\n";
  
  llvm::AllocaInst* allocate_start;
  llvm::AllocaInst* allocate_end;
  llvm::LoadInst* load_original;

  Value* phi_ind_node;
  PHINode* phi_ind_var;
  Value* phi_upper_node;
  Value* phi_ind_var_next;

  phi_ind_node = dyn_cast<PHINode>(ind_inst);

  allocate_start = prehead.CreateAlloca(llvm::IntegerType::getInt64Ty(CTX), nullptr ,"iterator");
  allocate_end = prehead.CreateAlloca(llvm::IntegerType::getInt64Ty(CTX),nullptr, "iterator_bound");
  prehead.CreateStore(phi_ind_node, allocate_start, false);
  if(!upper_bound_phi){
    Value* upper_bound_val = prehead.CreateLoad(llvm::IntegerType::getInt32Ty(CTX), upper_inst, "");
    phi_upper_node = prehead.CreateSExt(upper_bound_val, llvm::IntegerType::getInt64Ty(CTX), "value_sext");
    prehead.CreateStore(phi_upper_node, allocate_end, false);
  }

  else {
    phi_upper_node = dyn_cast<PHINode>(upper_inst);
    prehead.CreateStore(phi_upper_node, allocate_end, false);
  }

  prehead.CreateBr(InnerLoopHeader);

  IRBuilder<> head(InnerLoopHeader);

  phi_ind_var = head.CreatePHI(llvm::IntegerType::getInt64Ty(CTX), 0, "");
  phi_ind_var->addIncoming(phi_ind_node, InnerLoopPreheader);
  //Value* upper_var = head.CreateLoad(llvm::IntegerType::getInt64Ty(CTX), allocate_end, "");
  errs()<<"--- alloca works---\n";
  // //llvm::StoreInst* store_start = prehead.CreateStore(phi_ind_node,allocate_start,false);
  // // llvm::LoadInst* load_start =  prehead.CreateLoad(llvm::IntegerType::getInt64Ty(CTX), allocate_start, "");
  // errs()<<"---- store works----\n";
  // llvm::Value* ind_end = head.CreateNSWAdd(end_ci, phi_ind_node,"");
  // errs()<<"------ create NSWADD works----\n";
  // std::string str10;
  // raw_string_ostream stream10(str10);
  // ind_end->getType()->print(stream10,false);
  // errs()<<"ind end type is "<<str10<<"\n";
  // //head.CreateStore(ind_end, allocate_end, false);
  // errs()<<"this is the end \n";

  // // Value* boolValue = ConstantInt::get(Type::getInt1Ty(CTX), 1);
  // // //Value* cmpvalue = prehead.CreateICmp
  // // //prehead.CreateBr(InnerLoopHeader);
  // // prehead.CreateCondBr(boolValue, InnerLoopHeader, InnerLoopPreheader);

  // // errs()<<"------------- this works------------\n";

  // llvm::Value* compare;
  // //llvm::LoadInst* load_ind = head.CreateLoad(llvm::IntegerType::getInt64Ty(CTX),allocate_start,"inner_itr_start");
  // //llvm::LoadInst* load_ind_end = head.CreateLoad(llvm::IntegerType::getInt64Ty(CTX),allocate_end,"inner_itr_end");
  // // inner loop iterator is new so has to be loaded irrespective of outer loop iterator
  // //llvm::Value* ind_end = prehead.CreateNSWAdd(end_ci, originalInd,"");
  // compare = head.CreateICmpSLE(upper_var,ind_end);
  // head.CreateCondBr(compare, TempCompare, LatchBlock); // if true execute the original body of loop else move to latch of original
  // //newLoopBlocks[0],

  // IRBuilder<> tempblock(TempCompare);
  // llvm::LoadInst* load_outer_bound;
  // llvm::Value* check;
  // llvm::LoadInst* load_ind_start;

  
  // //load_ind_start = tempblock.CreateLoad(llvm::IntegerType::getInt64Ty(CTX),allocate_start,"inner_itr_end");

  // std::string str12;
  // raw_string_ostream stream12(str12);
  // phi_upper_node->getType()->print(stream12,false);
  // errs()<<"upper bound type is "<<str12<<"\n";

  // load_outer_bound = tempblock.CreateLoad(llvm::IntegerType::getInt32Ty(CTX), upper_inst,"");
  // phi_upper_node = tempblock.CreateSExt(load_outer_bound, llvm::IntegerType::getInt64Ty(CTX), "value_sext");
  // check = tempblock.CreateICmpSLE(phi_ind_node,phi_upper_node,"");

  // // else {
  // //   load_outer_bound = tempblock.CreateLoad(llvm::IntegerType::getInt32Ty(CTX), upperBound,"");
  // // }

  // tempblock.CreateCondBr(check, newLoopBlocks[0], LatchBlock);
  // errs()<<"--------- still an issue 2 ------------\n";

  head.CreateBr(newLoopBlocks[0]);

  // change the branch instruction of last block of newloopblocks also add 1 to iterator and also the original header (also value of the header)

  llvm::Instruction *Linst = Header->getTerminator();
  if(Linst){
    if(BranchInst* BTerm = dyn_cast<BranchInst> (Linst)){
      BasicBlock *exit;
      if(BTerm->isConditional()){
        exit = BTerm->getSuccessor(1);
      }

      else {
        exit = BTerm->getSuccessor(0);
      }

      Linst->eraseFromParent();
      // llvm::Instruction *Linst_temp = Header->getTerminator();
      // if(Linst_temp){ // this will remain NULL not the previous Instruction
      //   cout<<"works"<<endl;
      //   //Value* v = LI->getOperand(0);  
      // }

      llvm::BranchInst::Create(InnerLoopPreheader,exit,temp_v,Header);
      //llvm::BranchInst::Create(InnerLoopPreheader,Header);
      //Header->getInstList.push_back(I)
    }

    else {
      errs() <<" ----- not a branch instruction ----\n";
    }
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
  //llvm::LoadInst *ind_var = latch.CreateLoad(llvm::IntegerType::getInt64Ty(CTX),allocate_start,"store_start");
  std::cout<<"----------------------------------works till here--------------------"<<std::endl;
  phi_ind_var_next = latch.CreateNSWAdd(phi_ind_var, itr_ci,"increment");
  //latch.CreateStore(bio,allocate_start,false);
  Value *upper_bound_var = latch.CreateLoad(llvm::IntegerType::getInt64Ty(CTX),allocate_end,"store_start");
  Value *check = latch.CreateICmpSLE(phi_ind_var_next,upper_bound_var,"");
  latch.CreateCondBr(check, TempCompare, LatchBlock);  // this is created to maintain systematic loop formation of llvm

  IRBuilder<> tempblock(TempCompare);
  Value* ind_end = tempblock.CreateNSWAdd(end_ci, phi_ind_var,"");
  Value *check_2 = tempblock.CreateICmpSLE(phi_ind_var_next,ind_end,"");
  tempblock.CreateCondBr(check_2, InnerLoopHeader, LatchBlock);
  phi_ind_var->addIncoming(phi_ind_var_next, TempCompare);


  for(int i = 0 ; i < newLoopBlocks.size(); i++){
    for(llvm::BasicBlock::iterator I = newLoopBlocks[i]->begin(), Iend = newLoopBlocks[i]->end(); I != Iend ; ++I){
      for(Use &operand: I->operands()){
        if(operand.get() == phi_ind_node){
          operand.set(phi_ind_var); // or allocate_start?
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

void getInductionVariableUsingCmp(Loop *L, ScalarEvolution *SE){
  BasicBlock *header = L->getHeader();
  BasicBlock *latch = L->getLoopLatch();

  errs() << "executing get induction variable\n";

  PHINode *IndVar = L->getInductionVariable(*SE);

  errs() << "executed get induction variable\n";

  if(IndVar == nullptr){
    errs() <<"null ptr received\n";
    return;
  }

  if(BranchInst *BI = dyn_cast<BranchInst>(latch->getTerminator())){
    errs()<<"latch branch instruction found\n";
    if(BI->isConditional()){
      errs()<<"conditional branch\n";
      Instruction *I = dyn_cast<ICmpInst>(BI->getCondition());
      if(SExtInst *se = dyn_cast<SExtInst>(I->getOperand(1))){
        if(LoadInst *LI = dyn_cast<LoadInst>(se->getOperand(0))){
          Value *upper_val = LI->getOperand(0);
          errs() <<"loop split with signed extension\n";
          LoopSplit(L, 2, L->getExitBlock(), IndVar, upper_val, I, false, true);
        }

        else {
          errs() <<"invalid upper bound format\n";
        }
      }

      else if (PHINode *UpperVar = dyn_cast<PHINode>(I->getOperand(1))){
       errs() <<"upper bound is a phi node\n"; 
       LoopSplit(L, 2, L->getExitBlock(), IndVar, UpperVar, I, true, true);
      }

      else {
        errs() <<"invalid upper bound format no sext and phi\n";
      }
    }
  }

  return;
}

PreservedAnalyses TtexLoadToPhiPass::run(Module &M, ModuleAnalysisManager &MA) {

  errs()<<"----------------- ttex load to phi found-------------\n";

  int optioValue = MyOption;

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
                getInductionVariableUsingCmp(L,SE); 

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

  errs() <<"---------- completed the execution of the pass ----------- next is analysis ----------------\n";

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