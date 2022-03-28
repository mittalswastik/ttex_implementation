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
using namespace llvm;

#define DEBUG_TYPE "hello"

STATISTIC(HelloCounter, "Counts number of functions greeted");

namespace {
  // Hello - The first implementation, without getAnalysisUsage.
  struct LoopSplit : public LoopPass {
    static char ID; // Pass identification, replacement for typeid
    LoopSplit() : LoopPass(ID) {}

    bool runOnLoop(Loop *L, LPPassManager &LPM) override {
      return false;
    }
  };
}

char LoopSplit::ID = 0;
static RegisterPass<LoopSplit> X("split", "Loop Split Pass");

namespace {
  struct ReadArray : public FunctionPass {
    static char ID;
    ReadArray() : FunctionPass(ID) {}

    std::vector< std::vector<llvm::Constant*> >temp;

    bool runOnFunction(Function &F) override {

      if(F.getName() == ".omp_outlined."){
        std::vector<llvm::Constant*> temp2;
        MDNode* ttex_array = F.getMetadata("ttex_array");
          //ValueAsMetadata ttex_array = F.getMetadata("ttex_array");
        if(ttex_array){
          Value* v = dyn_cast<ValueAsMetadata> (ttex_array->getOperand(0))->getValue();
          if(v){
            ConstantDataArray* init = dyn_cast<ConstantDataArray> (v);
            //std::vector<Constant*> arr_temp = (init->getOperand(0))->getValue(); 
            std::vector<uint64_t> temp;
            if(init){
              int n = init->getNumElements();
              for(unsigned i = 0 ; i < n ; i++){
                temp.push_back(init->getElementAsInteger(i));
                errs()<<"value is: "<<temp[i]<<"\n";
              }
            }
          }

          Type* n = v->getType();
          std::string test;
          raw_string_ostream stream(test);
          n->print(stream);
          std::cout<<test<<std::endl;
          stream.flush();

        }
      }

      return false;
    }
  };
}

char ReadArray::ID = 0;
static RegisterPass<ReadArray> T("readarr", "read array metadata");

namespace {
  // Hello2 - The second implementation with getAnalysisUsage implemented.
  struct DataStructure : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    DataStructure() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {

      LLVMContext &llvm_context = M.getContext();

      errs()<<"checking"<<"\n";

      GlobalVariable* G = M.getGlobalVariable("info");
      Type* T = G->getType();
      Type* tp = T->getContainedType(0);//(T->getPointerTo())->getElementType();
      std::string test;
      raw_string_ostream stream(test);
      tp->print(stream);
      std::cout<<test<<std::endl;
      stream.flush();
      //return false;

      for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
          
          Function &F = *func_iter;

          if (!F.isDeclaration()) {
              if(F.getName() == "ompt_start_tool"){
                BasicBlock* B; // expect only one basic block
                B = &*(F.begin());

                //AllocaInst* arg_alloc = builder.CreateAlloca(T);//builder is IRBuilder
                // if(T->isPointerTy()){
                //   Type* tpp = T->getPointerElementType();
                //   Type* ITy = Type::getInt32Ty(llvm_context);
                //   Constant* AllocSize = ConstantExpr::getSizeOf(tpp);
                //   AllocSize = ConstantExpr::getTruncOrBitCast(AllocSize, ITy);
                //   CallInst::CreateMalloc(B->getTerminator(), ITy, tpp, AllocSize, nullptr,nullptr,"");
                // }
      


                // PointerType* ptr = T->getPointerTo();
                // ArrayType* arr_details = ArrayType::get(T,2); // 5 will be value from metadata
                // llvm::ConstantInt *sub_region_size = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(llvm_context),2, false); //metadata value
                // //AllocaInst* allocate_info_struct = new AllocaInst(ptr,0,"info_struct",B->getTerminator());
                
                // //AllocaInst* allocate_arr_details = new AllocaInst(arr_details,0,"arr_details",B->getTerminator());
                
                // Constant* allocate_arr_details_sub = ConstantExpr::getSizeOf(arr_details);
                // //PointerType* ptr = T->getPointerTo();
                // Type* ITy = Type::getStructElementType(4);
                // allocate_arr_details_sub = ConstantExpr::getTruncOrBitCast(allocate_arr_details_sub, arr_details);
                // //Instruction *malloced = CallInst::CreateMalloc(B->getTerminator(), T, allocate_arr_details->getAllocatedType(), allocate_arr_details,nullptr,nullptr,"");
                // //Instruction *malloced = CallInst::CreateMalloc(B->getTerminator(), T, allocate_arr_details->getAllocatedType(), allocate_arr_details, allocate_arr_details->getArraySize(),nullptr,"");
                // //Instruction *malloced = CallInst::CreateMalloc(B->getTerminator(), T, arr_details, allocate_arr_details_sub, nullptr, nullptr, "malloced");
                

                /*works*/

                //Value *arraysize = ConstantInt::get(Int8PtrTy, 2);
                //ArraySize = ConstantInt::get(IntPtrTy, 1);
                // ArrayType* arr_details = ArrayType::get(tp->getContainedType(0),5);
                // Constant* allocsize = ConstantExpr::getSizeOf(arr_details);
                // //Constant* allocsize = ConstantExpr::getSizeOf(tp->getContainedType(0));
                // allocsize = ConstantExpr::getTruncOrBitCast(allocsize, Type::getInt64PtrTy(B->getContext()));
                // Instruction *malloced = CallInst::CreateMalloc(B->getTerminator(), Type::getInt64PtrTy(B->getContext()), tp->getContainedType(0), allocsize, nullptr, nullptr, "malloced");
                

                //^ above gives a seg fault error

                /*works end*/
                errs()<<"checking"<<"\n";

                ConstantInt *arraysize = ConstantInt::get(Type::getInt64Ty(B->getContext()), 2);
                Constant* allocsize = ConstantExpr::getSizeOf(tp->getContainedType(0));
                allocsize = ConstantExpr::getTruncOrBitCast(allocsize, Type::getInt64Ty(B->getContext()));
                Instruction *malloced = CallInst::CreateMalloc(B->getTerminator(), Type::getInt64Ty(B->getContext()), tp->getContainedType(0), allocsize, arraysize, nullptr, "malloced");
                


                Instruction *storeinfo = new StoreInst(malloced,M.getGlobalVariable("info"),B->getTerminator());
                errs()<<"checking"<<"\n";
                Instruction *loadinfo = new LoadInst(tp,M.getGlobalVariable("info"),"",B->getTerminator());
                std::vector<llvm::Value*> indices;
                //indices.push_back(llvm::ConstantInt::get(llvm_context, llvm::APInt(64, 0, false)));
                indices.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),1, false));
                //indices.push_back(llvm::ConstantInt::get(llvm_context, llvm::APInt(64, 0, false)));
                GetElementPtrInst *gepinst = GetElementPtrInst::Create(tp->getContainedType(0),loadinfo,indices,"",B->getTerminator());
                
                //createInBounds is not needed for getelementptr

                //indices.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),0, false));
                //GetElementPtrInst *gepinst_2 = GetElementPtrInst::CreateInBounds(tp->getContainedType(0),gepinst,indices,"",B->getTerminator());
                //indices.pop_back();
                std::vector<llvm::Value*> indices_2;
                indices_2.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),0, false));
                indices_2.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(B->getContext()),1, false)); // this i64 will give access error as struct value accessed is int
                
                // GlobalVariable* G1 = M.getGlobalVariable("info");
                // Type* T1 = G1->getType();
                // Type* tp1 = T1->getContainedType(0);//(T->getPointerTo())->getElementType();
                // std::string test;
                // raw_string_ostream stream2(test);
                // tp1->getContainedType(0)->print(stream2);
                // std::cout<<test<<std::endl;
                // stream2.flush();

                //indices_2.push_back(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(B->getContext()),0, false));
                GetElementPtrInst *gepinst_3 = GetElementPtrInst::Create(tp->getContainedType(0),gepinst,indices_2,"",B->getTerminator());
                new StoreInst(llvm::ConstantInt::get(llvm_context, llvm::APInt(32, 9, false)),gepinst_3,B->getTerminator());

                // std::vector<llvm::Value*> indices;
                // indices.push_back(llvm::ConstantInt::get(llvm_context, llvm::APInt(64, 0, false)));
                // indices.push_back(llvm::ConstantInt::get(llvm_context, llvm::APInt(64, 0, false)));
                //indices.push_back(llvm::ConstantInt::get(llvm_context, llvm::APInt(64, 0, false)));
                //AllocaInst* allocate_wcet_arr = new AllocaInst(llvm::IntegerType::getInt64Ty(llvm_context),0,"wcet_arr",B->getTerminator());
                //indices.push_back(allocate_wcet_arr);
                
                //Type* type_arr_details = allocate_arr_details->getAllocatedType();
                
                //GetElementPtrInst *gepinst = GetElementPtrInst::Create(type_arr_details,allocate_arr_details,indices,"set_structure");
                
                //ArrayType* parallel_det = ArrayType::get(type_arr_details,5);
                //AllocaInst* allocate_parallel_arr = new AllocaInst(parallel_det,sizeof(parallel_det),"parallel_det",B->getTerminator());
              }
          }    
      }    
      
      return false;

    }

    // We don't modify the program, so we preserve all analyses.
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
    }
  };
}

char DataStructure::ID = 0;
static RegisterPass<DataStructure>
Y("datastructure", "Hello World Pass (with getAnalysisUsage implemented)");


namespace {
  // Hello - The first implementation, without getAnalysisUsage.
  struct  PassId : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    PassId() : FunctionPass(ID) {}

    // PassId() : FunctionPass(ID) {
    //   initializePassIdPass(*PassRegistry::getPassRegistry());
    // }
    // explicit PassId(bool FatalErrors)
    //     : FunctionPass(ID),
    //       FatalErrors(FatalErrors) {
    //   initializePassIdPass(*PassRegistry::getPassRegistry());
    // }

    bool runOnFunction(Function &F) override {

      if (!F.isDeclaration()) {
        if(F.getName() == ".omp_outlined."){

          Module *M = F.getParent();
          llvm::LLVMContext &CTX = M->getContext();

          for (Function::iterator block_iter = F.begin(), block_iter_end = F.end(); block_iter != block_iter_end; ++block_iter) {
            BasicBlock &B = *block_iter;
            for(BasicBlock::iterator instr_iter = B.begin(), instr_iter_end = B.end(); instr_iter != instr_iter_end; ++instr_iter){
              Instruction &I = *instr_iter;
              if(CallInst* call_inst = dyn_cast<CallInst>(&I)){
                 Function* fn = call_inst->getCalledFunction();
                 errs()<<"Function name: "<<fn->getName()<<"\n";

                 // errs()<<"Call instruction get argument of the function: "<<call_inst->getOperand(100)<<"\n";
                 if(fn->getName() == "__kmpc_for_static_init_4"){
                    //for(auto arg = fn->arg_begin(); arg != fn->arg_end(); ++arg) {
                      llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),10, false);
                      call_inst->setOperand(9,itr_ci);
                      Argument* arg = fn->arg_end()-1;
                      //llvm::ConstantInt *itr_ci = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(CTX),1, false);
                      //Argument* atemp = arg->getArgument();
                      //arg = itr_ci;
                      errs()<<"argument number is: "<<arg->getArgNo()<<"\n";
                      Type* t = arg->getType();
                      //t->dump();
                      std::string test_temp;
                      raw_string_ostream stream3(test_temp);
                      t->print(stream3);
                      std::cout<<test_temp<<std::endl;
                      stream3.flush();
                      errs()<<"\n name of the value is: "<<arg->getName()<<"\n";

                      if(auto* ci = dyn_cast<Constant>(arg))
                        //errs() << ci->getValue() << "\n"; 
                        {
                          errs()<<"right direction"<<"\n";
                          // std::string test;
                          // raw_string_ostream stream(test);
                          // arg->dump(stream);
                          // std::cout<<test<<std::endl;
                          // stream.flush();
                          //errs() <<ci->dump() <<"\n";
                        }
                      //errs() << *arg << "\n";
                    //}
                 }
              }

            }
          }
        }
      }  

      return false;
    }
  };
}

char PassId::ID = 0;
static RegisterPass<PassId> Z("passid", "get region id pass");

// new pass code



// end of new pass code

// static RegisterStandardPasses Y(
//     PassManagerBuilder::EP_EarlyAsPossible,
//     [](const PassManagerBuilder &Builder,
//        legacy::PassManagerBase &PM) { PM.add(new Hello()); });


// static void RegisterPass(const PassManagerBuilder &,
//                          legacy::PassManagerBase &PM) {
//   PM.add(new DataStructure());
// }


// namespace llvm {
//   FunctionPass *
//   llvm::createPassIdPass() {
//     return new PassId();
//   }

  // ModulePass *
  // llvm::createDataStructurePass() {
  //   return new DataStructure();
  // }

//}



// INITIALIZE_PASS_BEGIN(DataStructure, "datastructure", 
//                       "Some description for the Pass", 
//                       false, false)
// //INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass) // Or whatever your Pass dependencies
// INITIALIZE_PASS_END(DataStructure, "datastructure",
//                     "Some description for the Pass", 
//                     false, false)


/*


  Use the read array to create structure values (Basically create new global)


*/