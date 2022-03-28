#ifndef LLVM_TRANSFORMS_SCALAR_TTEXPASS_H
#define LLVM_TRANSFORMS_SCALAR_TTEXPASS_H

#include "llvm/Pass.h"

namespace llvm {
ModulePass* createTtexPass();
} // end namespace llvm

#endif