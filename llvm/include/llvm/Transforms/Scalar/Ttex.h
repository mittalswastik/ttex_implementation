#ifndef LLVM_TRANSFORMS_SCALAR_TTEXPASS_H
#define LLVM_TRANSFORMS_SCALAR_TTEXPASS_H

#include "llvm/Pass.h"

namespace llvm {
ModulePass* createTtexPass(std::vector<std::string>);
} // end namespace llvm

#endif