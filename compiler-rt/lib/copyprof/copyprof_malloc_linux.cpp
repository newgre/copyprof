#include "copyprof/copyprof_interface_internal.h"
#include "copyprof/copyprof_internal.h"
#include "copyprof_interceptors.h"
#include "interception/interception.h"
#include "sanitizer_common/sanitizer_allocator_dlsym.h"

// TODO: b/422734209 - create a global allocator / deallocator based around
// malloc/free that supports aligned allocation, and use it for operator
// new/delete as well as the intercepted malloc/free, then get rid of
// `CombinedAllocator<>`.

// TODO: b/422734209 - implement support for additional libc heap functions.

using namespace __copyprof;
using namespace __sanitizer;

namespace __copyprof {

// This is needed to make the malloc/free interceptros re-entrancy safe during
// the startup phase as `dlsym` itself might allocate.
struct DlsymAlloc : public DlSymAllocator<DlsymAlloc> {
  static bool UseImpl() { return !copyprof_is_initialized; }
};

}  // namespace __copyprof

#define ENSURE_COPYPROF_INITIALIZED()     \
  if (UNLIKELY(!copyprof_is_initialized)) \
  __copyprof_init()

INTERCEPTOR(void*, malloc, SIZE_T size) {
  if (DlsymAlloc::Use())
    return DlsymAlloc::Allocate(size);
  return REAL(malloc)(size);
}

INTERCEPTOR(void, free, void* ptr) {
  if (DlsymAlloc::PointerIsMine(ptr))
    return DlsymAlloc::Free(ptr);
  REAL(free)(ptr);
}

namespace __copyprof {

void InitializeHeapInterceptors() {
  COPYPROF_INTERCEPT_FUNC(malloc);
  COPYPROF_INTERCEPT_FUNC(free);
}

}  // namespace __copyprof