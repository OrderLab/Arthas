#ifndef _FUTURE_EXTRAS_H_
#define _FUTURE_EXTRAS_H_

#include "llvm/IR/Instructions.h"

namespace llvm {

/// A helper function that returns the pointer operand of a load or store
/// instruction. Returns nullptr if not load or store.
inline const Value *getLoadStorePointerOperand(const Value *V) {
  if (auto *Load = dyn_cast<LoadInst>(V))
    return Load->getPointerOperand();
  if (auto *Store = dyn_cast<StoreInst>(V))
    return Store->getPointerOperand();
  return nullptr;
}
inline Value *getLoadStorePointerOperand(Value *V) {
  return const_cast<Value *>(
      getLoadStorePointerOperand(static_cast<const Value *>(V)));
}

/// A helper function that returns the pointer operand of a load, store
/// or GEP instruction. Returns nullptr if not load, store, or GEP.
inline const Value *getPointerOperand(const Value *V) {
  if (auto *Ptr = getLoadStorePointerOperand(V))
    return Ptr;
  if (auto *Gep = dyn_cast<GetElementPtrInst>(V))
    return Gep->getPointerOperand();
  return nullptr;
}
inline Value *getPointerOperand(Value *V) {
  return const_cast<Value *>(getPointerOperand(static_cast<const Value *>(V)));
}

/// A helper function that returns the alignment of load or store instruction.
inline MaybeAlign getLoadStoreAlignment(Value *I) {
  assert((isa<LoadInst>(I) || isa<StoreInst>(I)) &&
         "Expected Load or Store instruction");
  if (auto *LI = dyn_cast<LoadInst>(I))
    return MaybeAlign(LI->getAlignment());
  return MaybeAlign(cast<StoreInst>(I)->getAlignment());
}

/// A helper function that returns the address space of the pointer operand of
/// load or store instruction.
inline unsigned getLoadStoreAddressSpace(Value *I) {
  assert((isa<LoadInst>(I) || isa<StoreInst>(I)) &&
         "Expected Load or Store instruction");
  if (auto *LI = dyn_cast<LoadInst>(I))
    return LI->getPointerAddressSpace();
  return cast<StoreInst>(I)->getPointerAddressSpace();
}

}

#endif /* _FUTURE_EXTRAS_H_ */
