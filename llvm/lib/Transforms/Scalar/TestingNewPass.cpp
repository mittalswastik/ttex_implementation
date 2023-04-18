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

std::vector< std::vector< std::pair<int,int> > > astdata; //storing the sub region info -- but need to fix the id's to correct location
std::vector<int> sizes;
std::vector< std::vector<int> > loop_split;

bool maxvuln_set = true;

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

    // if(BranchInst *binst = dyn_cast<BranchInst>(header->getTerminator())){
    //   if(BasicBlock *compare_block = dyn_cast<BasicBlock> (binst->getOperand(1))){ // exiting block of the loop
    //     //LoopSplit(ltemp, loop_split[parallel_region_id][ctr], compare_block, parallel_region_id, ctr);
    //     split_check = LoopSplit(ltemp, 100, compare_block, parallel_region_id, ctr); // runs ompt_test as many times as the iteration (a lot)
    //   }
    // }

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