#include "llvm/Transforms/Instrumentation/CopyProf.h"

#include "stdint.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include <array>
#include <cstddef>

using namespace llvm;

#define DEBUG_TYPE "copyprof"

using namespace llvm;

namespace {

constexpr StringRef kCopyProfModuleCtorName = "copyprof.module_ctor";
constexpr StringRef kCopyProfInitName = "__copyprof_init";

constexpr StringRef kCopyProfCtorAttr = "copyprof-ctor";
constexpr StringRef kCopyProfCopyCtorAttr = "copyprof-copy-ctor";
constexpr StringRef kCopyProfCopyAssignAttr = "copyprof-copy-assign-op";
constexpr StringRef kCopyProfDtorAttr = "copyprof-dtor";

// TODO(jannewger): how to add copyprof-ctor attribute to =default c'tors (of
// trivial types)? we don't even see them as declarations in the codegen.

// The basic copy profile algorithm works like this:
// An object X is copied to Y. The shadow memory corresponding to Y is
// initalized as "copied". All stores to an object marks its corresponding
// shadow memory as "modified". When an object is destroyed and all of its
// shadow memory is marked as "copied", the object is reported as an unnecessary
// copy.
// TODO(jannewger): handle the following cases:
// 1) object X is copied to Y, then X is modified and Y is observed. Y should
// probably not be reported as an unnecessary copy.

void insertModuleCtor(Module &M) {
  getOrCreateSanitizerCtorAndInitFunctions(
      M, kCopyProfModuleCtorName, kCopyProfInitName,
      /*InitArgTypes=*/{},
      /*InitArgs=*/{},
      // This callback is invoked when the functions are created the first
      // time. Hook them into the global ctors list in that case:
      [&](Function *Ctor, FunctionCallee) { appendToGlobalCtors(M, Ctor, 0); });
}

class CopyProf {
public:
  CopyProf(Module &M);
  bool sanitizeFunction(Function &F);

private:
  void handleCtor(Function &F, size_t ObjSize);
  void handleCopyCtor(Function &F, size_t ObjSize);
  void handleCopyAssignOp(Function &F, size_t ObjSize);
  void handleDtor(Function &F, size_t ObjSize);
  void insertCallback(Function &F, size_t ObjSize, unsigned NumArgs,
                      FunctionCallee Callback, FunctionCallee ExitCallback);

  LLVMContext *C;
  Type *IntPtrTy;
  FunctionCallee CtorEnterCallback;
  FunctionCallee CtorExitCallback;
  FunctionCallee CopyCtorEnterCallback;
  FunctionCallee CopyCtorExitCallback;
  FunctionCallee CopyAssignOpEnterCallback;
  FunctionCallee CopyAssignOpExitCallback;
  FunctionCallee DtorEnterCallback;
  FunctionCallee DtorExitCallback;
  FunctionCallee StoreCallback;
};

CopyProf::CopyProf(Module &M) {
  C = &M.getContext();
  IRBuilder<> IRB(*C);
  IntPtrTy = IRB.getIntPtrTy(M.getDataLayout());
  Type *PtrTy = IRB.getPtrTy();
  Type *VoidTy = IRB.getVoidTy();
  CtorEnterCallback = M.getOrInsertFunction("__copyprof_ctor_enter_callback",
                                            VoidTy, PtrTy, IntPtrTy);
  CtorExitCallback = M.getOrInsertFunction("__copyprof_ctor_exit_callback",
                                           VoidTy, PtrTy, IntPtrTy);
  CopyCtorEnterCallback = M.getOrInsertFunction(
      "__copyprof_copy_ctor_enter_callback", VoidTy, PtrTy, PtrTy, IntPtrTy);
  CopyCtorExitCallback = M.getOrInsertFunction(
      "__copyprof_copy_ctor_exit_callback", VoidTy, PtrTy, PtrTy, IntPtrTy);
  CopyAssignOpEnterCallback =
      M.getOrInsertFunction("__copyprof_copy_assign_op_enter_callback", VoidTy,
                            PtrTy, PtrTy, IntPtrTy);
  CopyAssignOpExitCallback =
      M.getOrInsertFunction("__copyprof_copy_assign_op_exit_callback", VoidTy,
                            PtrTy, PtrTy, IntPtrTy);
  DtorEnterCallback = M.getOrInsertFunction("__copyprof_dtor_enter_callback",
                                            VoidTy, PtrTy, IntPtrTy);
  DtorExitCallback = M.getOrInsertFunction("__copyprof_dtor_exit_callback",
                                           VoidTy, PtrTy, IntPtrTy);
  StoreCallback = M.getOrInsertFunction("__copyprof_store_callback", VoidTy,
                                        PtrTy, IntPtrTy);
}

// Returns the object size in bytes that was stored in the given function
// attribute during parsing in the frontend.
size_t GetAttrValueAsInt(const Function &F, StringRef Attr) {
  size_t IntValue = 0;
  if (!to_integer<size_t>(F.getFnAttribute(Attr).getValueAsString(), IntValue,
                          /*Base=*/10)) {
    report_fatal_error(formatv("Unable to parse integer value from function "
                               "attribute value in '{0}': {1}:{2}",
                               F.getName(), Attr,
                               F.getFnAttribute(Attr).getValueAsString()));
  }
  return IntValue;
}

bool CopyProf::sanitizeFunction(Function &F) {
  // Make sure to not instrument our own module c'tor.
  if (F.getName() == kCopyProfModuleCtorName) {
    return false;
  }
  if (F.hasFnAttribute(Attribute::DisableSanitizerInstrumentation)) {
    return false;
  }

  if (F.hasFnAttribute(kCopyProfCtorAttr)) {
    handleCtor(F, GetAttrValueAsInt(F, kCopyProfCtorAttr));
  } else if (F.hasFnAttribute(kCopyProfCopyCtorAttr)) {
    handleCopyCtor(F, GetAttrValueAsInt(F, kCopyProfCopyCtorAttr));
  } else if (F.hasFnAttribute(kCopyProfCopyAssignAttr)) {
    handleCopyAssignOp(F, GetAttrValueAsInt(F, kCopyProfCopyAssignAttr));
  } else if (F.hasFnAttribute(kCopyProfDtorAttr)) {
    handleDtor(F, GetAttrValueAsInt(F, kCopyProfDtorAttr));
  }

  return true;
}

void CopyProf::insertCallback(Function &F, size_t ObjSize, unsigned NumArgs,
                              FunctionCallee EntryCallback,
                              FunctionCallee ExitCallback) {
  auto InsertCallback = [IntPtrTy = IntPtrTy, ObjSize,
                         NumArgs](Function &F, IRBuilder<> &&IRB,
                                  FunctionCallee Callback) {
    SmallVector<Value *, 3> Args;
    // `this` is always the first argument to a special member function, but
    // copy c'tor / copy assignment operator will have the other `this` ptr
    // passed as their second argument.
    assert(NumArgs == 1 || NumArgs == 2);
    for (unsigned I = 0; I < NumArgs; ++I) {
      Args.push_back(IRB.CreatePointerCast(F.getArg(I), IRB.getPtrTy()));
    }
    // The last argument to the callback is the size of the object pointed at by
    // `this`.
    Args.push_back(ConstantInt::get(IntPtrTy, ObjSize));
    IRB.CreateCall(Callback, Args);
  };

  InsertCallback(
      F, IRBuilder<>{&F.getEntryBlock(), F.getEntryBlock().getFirstNonPHIIt()},
      EntryCallback);
  for (BasicBlock &BB : F) {
    if (ReturnInst *Ret = dyn_cast<ReturnInst>(BB.getTerminator());
        Ret != nullptr) {
      InsertCallback(F, IRBuilder<>{Ret}, ExitCallback);
    }
  }
}

void CopyProf::handleCtor(Function &F, size_t ObjSize) {
  insertCallback(F, ObjSize, /*NumArgs=*/1, CtorEnterCallback,
                 CtorExitCallback);
}

void CopyProf::handleCopyCtor(Function &F, size_t ObjSize) {
  insertCallback(F, ObjSize, /*NumArgs=*/2, CopyCtorEnterCallback,
                 CopyCtorExitCallback);
}

void CopyProf::handleCopyAssignOp(Function &F, size_t ObjSize) {
  insertCallback(F, ObjSize, /*NumArgs=*/2, CopyAssignOpEnterCallback,
                 CopyAssignOpExitCallback);
}

void CopyProf::handleDtor(Function &F, size_t ObjSize) {
  insertCallback(F, ObjSize, /*NumArgs=*/1, DtorEnterCallback,
                 DtorExitCallback);
}

// Late-stage store instrumentation.
// This class handles instrumenting store instructions after all optimizations
// have run, to avoid instrumenting stores that would be eliminated.
class CopyProfStores {
public:
  CopyProfStores(Module &M) {
    C = &M.getContext();
    IRBuilder<> IRB(*C);
    IntPtrTy = IRB.getIntPtrTy(M.getDataLayout());
    Type *PtrTy = IRB.getPtrTy();
    Type *VoidTy = IRB.getVoidTy();
    StoreCallback = M.getOrInsertFunction("__copyprof_store_callback", VoidTy,
                                          PtrTy, IntPtrTy);
  }

  bool instrumentFunction(Function &F) {
    if (F.hasFnAttribute(Attribute::DisableSanitizerInstrumentation))
      return false;

    const DataLayout &DL = F.getParent()->getDataLayout();
    bool Modified = false;
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (auto *SI = dyn_cast<StoreInst>(&I)) {
          uint64_t StoredSize =
              DL.getTypeStoreSize(SI->getValueOperand()->getType());
          IRBuilder<> IRB(SI);
          std::array<Value *, 2> Args = {
              IRB.CreatePointerCast(SI->getPointerOperand(), IRB.getPtrTy()),
              ConstantInt::get(IntPtrTy, StoredSize)};
          IRB.CreateCall(StoreCallback, Args);
          Modified = true;
        }
      }
    }
    return Modified;
  }

private:
  LLVMContext *C;
  Type *IntPtrTy;
  FunctionCallee StoreCallback;
};

} // namespace

PreservedAnalyses CopyProfPass::run(Function &F, FunctionAnalysisManager &FAM) {
  CopyProf CopyProf(*F.getParent());
  return CopyProf.sanitizeFunction(F) ? PreservedAnalyses::none()
                                      : PreservedAnalyses::all();
}

PreservedAnalyses ModuleCopyProfPass::run(Module &M,
                                          ModuleAnalysisManager &AM) {
  insertModuleCtor(M);
  return PreservedAnalyses::none();
}
PreservedAnalyses CopyProfStoresPass::run(Function &F,
                                          FunctionAnalysisManager &FAM) {
  CopyProfStores CopyProfStores(*F.getParent());
  return CopyProfStores.instrumentFunction(F) ? PreservedAnalyses::none()
                                              : PreservedAnalyses::all();
}
