#ifndef COPYPROF_ALLOCATOR_H
#define COPYPROF_ALLOCATOR_H

#include "copyprof_internal.h"
#include "sanitizer_common/sanitizer_internal_defs.h"
#include "sanitizer_common/sanitizer_stacktrace.h"

namespace __copyprof {

// Initializes the copyprof allocator at startup.
void InitializeCopyProfAllocator();

// Allocates memory using the copyprof allocator.
void* CopyProfAllocate(uptr size, uptr align, BufferedStackTrace* stack);
void CopyProfDeallocate(void* ptr, uptr size, uptr align);

}  // namespace __copyprof

#endif  // COPYPROF_ALLOCATOR_H
