#include "llvm/Transforms/Scalar/Sample.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include <iostream>
#include <bits/stdc++.h>
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/ADT/iterator.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/Loads.h"
#include "llvm/Analysis/PtrUseVisitor.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/ConstantFolder.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include "llvm/Transforms/Scalar/Ttex.h"
#include "llvm/IR/DataLayout.h"
using namespace llvm;

std::vector< std::vector<int> > astdata; //storing the sub region info -- but need to fix the id's to correct location
std::vector<int> sizes;


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
  
  errs()<<"--------------------------- printing sizes---------------------------"<<"\n";

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
    errs()<<"ASTDATA SIZE IS "<<astdata.size()<<"\n";

    for(int j = 0 ; j < astdata[i].size() ; j++){
      errs()<<"ast value is "<<astdata[i][j]<<" "<<i<<" "<<j<<"\n";
      std::vector<llvm::Value*> indices;
      indices.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),i, false));
      GetElementPtrInst *gepinst_2 = GetElementPtrInst::Create(T_2,load_para_region_1,indices,"",B->getTerminator());
      Instruction *load_sub_para_region = new LoadInst(T_2, gepinst_2,"",B->getTerminator());

      errs()<<"checking for error"<<"\n";

      std::vector<llvm::Value*> indices_2;
      indices.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),j, false));
      GetElementPtrInst *gepinst_3 = GetElementPtrInst::Create(T_3,load_sub_para_region,indices_2,"",B->getTerminator());

      errs()<<"checking for error2"<<"\n";

      std::vector<llvm::Value*> indices_3;
      indices_3.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),0, false));
      indices_3.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(B->getContext()),0, false)); // this i64 will give access error as struct value accessed is int
      GetElementPtrInst *gepinst_4 = GetElementPtrInst::Create(T_3,gepinst_3,indices_3,"",B->getTerminator());
      new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, astdata[i][j], true)),gepinst_4,B->getTerminator());
      // use option to see if maxvul is set

      // errs()<<"checking for error3"<<"\n";

      // std::vector<llvm::Value*> indices_4;
      // indices_4.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),0, false));
      // indices_4.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(B->getContext()),1, false)); // this i64 will give access error as struct value accessed is int
      // GetElementPtrInst *gepinst_5 = GetElementPtrInst::Create(T_3,gepinst_3,indices_4,"",B->getTerminator());
      // new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, j+1, true)),gepinst_5,B->getTerminator());

      // // load and read its value

      // errs()<<"checking for error4"<<"\n";

      // std::vector<llvm::Value*> indices_5;
      // indices_5.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),0, false));
      // indices_5.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(B->getContext()),2, false)); // this i64 will give access error as struct value accessed is int
      // GetElementPtrInst *gepinst_6 = GetElementPtrInst::Create(T_3,gepinst_3,indices_5,"",B->getTerminator());
      // new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, i+1, false)),gepinst_6,B->getTerminator());
    
      // errs()<<"checking for error5"<<"\n";
    }
  }
}

void setAstData(Module &M, Function &F, LLVMContext &CTX){

  std::vector<int> temp;
  MDNode* ttex_array = F.getMetadata("ttex_array");
  MDNode* parallel_id = F.getMetadata("parallel_id");

  if(ttex_array){
     // errs()<<"Retreiving num elements for each outlined 1"<<n<<"\n";
    Value* v = dyn_cast<ValueAsMetadata> (ttex_array->getOperand(0))->getValue();
    if(v){
      // errs()<<"Retreiving num elements for each outlined 2"<<n<<"\n";
      ConstantDataArray* init = dyn_cast<ConstantDataArray> (v);
      if(init){
        int n = init->getNumElements();
        for(unsigned i = 0 ; i < n ; i++){
          temp.push_back(init->getElementAsInteger(i));
        }

        errs()<<"Retreiving num elements for each outlined "<<temp.size()<<"\n";
      }
    }
  }

  // astdata.push_back(temp);
  // sizes.push_back(temp.size());

  Value* parallel_id_temp = dyn_cast<ValueAsMetadata>(parallel_id->getOperand(0))->getValue();
  auto* ci = dyn_cast<ConstantInt>(parallel_id_temp);

  errs()<<"------------ parallel id value is------"<<ci->getZExtValue()<<"\n";
  errs()<<"astdata size and sizes size: "<<astdata.size()<<" "<<sizes.size()<<"\n";

  astdata[ci->getZExtValue()-1] = temp;
  sizes[ci->getZExtValue()-1] = temp.size();

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

void updateWorkId(Module &M, Function &F, LLVMContext &CTX){
  int ctr = 1;
  for (Function::iterator block_iter = F.begin(), block_iter_end = F.end(); block_iter != block_iter_end; ++block_iter) {
    BasicBlock &B = *block_iter;
    for(BasicBlock::iterator instr_iter = B.begin(), instr_iter_end = B.end(); instr_iter != instr_iter_end; ++instr_iter){
      Instruction &I = *instr_iter;
      if(CallInst* call_inst = dyn_cast<CallInst>(&I)){
         Function* fn = call_inst->getCalledFunction();
         if(fn->getName() == "__kmpc_for_static_init_4"){
            llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
            call_inst->setOperand(9,itr_ci);
            ctr++;
         }

         else if(fn->getName() == "__kmpc_single"){
            llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),ctr, false);
            call_inst->setOperand(2,itr_ci);
            ctr++;
         }

         setAstData(M,F,CTX);
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

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
      AU.addRequired<LoopInfoWrapperPass>();
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