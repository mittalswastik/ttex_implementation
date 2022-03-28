#ifndef LLVM_TRANSFORMS_SCALAR_HELLONEWPMPASS_H
#define LLVM_TRANSFORMS_SCALAR_HELLONEWPMPASS_H

#include "llvm/Pass.h"

namespace llvm {
FunctionPass* createHelloNewPMPass();
} // end namespace llvm

#endif