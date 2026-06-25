#ifndef LLVM_TRANSFORMS_INSTRUMENTATION_COPYPROF_H
#define LLVM_TRANSFORMS_INSTRUMENTATION_COPYPROF_H

#include "llvm/IR/PassManager.h"

namespace llvm {

// Early-stage pass that inserts callbacks into special member functions.
class CopyProfPass : public PassInfoMixin<CopyProfPass> {
public:
  CopyProfPass() = default;
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);

  static bool isRequired() { return true; }
};

class ModuleCopyProfPass : public PassInfoMixin<ModuleCopyProfPass> {
public:
  ModuleCopyProfPass() = default;
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

  static bool isRequired() { return true; }
};

// Late-stage pass that instruments store instructions to detect whether an
// object copy has been modified before it is destructed.
class CopyProfStoresPass : public PassInfoMixin<CopyProfStoresPass> {
public:
  CopyProfStoresPass() = default;
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);

  static bool isRequired() { return true; }
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_INSTRUMENTATION_COPYPROF_H
