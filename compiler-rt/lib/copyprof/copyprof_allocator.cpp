#include "copyprof_allocator.h"

#include "copyprof_shadow.h"
#include "copyprof_state.h"
#include "sanitizer_common/sanitizer_allocator.h"
#include "sanitizer_common/sanitizer_allocator_checks.h"
#include "sanitizer_common/sanitizer_allocator_interface.h"
#include "sanitizer_common/sanitizer_allocator_report.h"
#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_errno.h"
#include "sanitizer_common/sanitizer_flags.h"
#include "sanitizer_common/sanitizer_internal_defs.h"
#include "sanitizer_common/sanitizer_platform_limits_posix.h"
#include "sanitizer_common/sanitizer_stacktrace.h"

namespace __copyprof {
namespace {

using namespace __sanitizer;

struct CopyProfMapUnmapCallback {
  void OnMap(uptr p, uptr size) const {}
  void OnMapSecondary(uptr p, uptr size, uptr user_begin,
                      uptr user_size) const {}
  void OnUnmap(uptr p, uptr size) const {}
};

struct CopyProfMetadata {
  // The requested allocation size in bytes.
  uptr requested_size;
};

struct AllocatorParameters {
  static constexpr uptr kSpaceBeg = 0x700000000000ULL;
  static constexpr uptr kSpaceSize = 0x40000000000ULL;
  static constexpr uptr kMetadataSize = sizeof(CopyProfMetadata);
  static constexpr uptr kFlags = 0;
  using SizeClassMap = DefaultSizeClassMap;
  using MapUnmapCallback = CopyProfMapUnmapCallback;
  using AddressSpaceView = LocalAddressSpaceView;
};

// TODO: move stuff into `CopyProfAllocator` struct , e.g., `allocator` as well
// as mutex and cache, also maxAllowedAllocSize etc.
struct CopyProfAllocator {};

using PrimaryAllocator = SizeClassAllocator64<AllocatorParameters>;
using Allocator = CombinedAllocator<PrimaryAllocator>;
using AllocatorCache = Allocator::AllocatorCache;

Allocator allocator;

// Fallback allocator cache and mutex used in the slow path when no per-thread
// allocator cache is available.
StaticSpinMutex fallback_mutex;
AllocatorCache fallback_allocator_cache;

CopyProfMetadata* GetMetadata(void* ptr) {
  return reinterpret_cast<CopyProfMetadata*>(allocator.GetMetaData(ptr));
}

}  // namespace

void InitializeCopyProfAllocator() {
  SetAllocatorMayReturnNull(common_flags()->allocator_may_return_null);
  allocator.Init(common_flags()->allocator_release_to_os_interval_ms);
  // TODO(jannewger): take common_flags()->max_allocation_size_mb into account
  // to set the max allocation size.
}

void* CopyProfAllocate(uptr size, uptr align, BufferedStackTrace* stack) {
  if (UNLIKELY(!IsPowerOfTwo(align))) {
    errno = errno_EINVAL;
    if (AllocatorMayReturnNull())
      return nullptr;
    ReportInvalidAllocationAlignment(align, stack);
  }
  // TODO(jannewger): implement per-thread allocator cache. Note that this also
  // requires proper "thread" support w/ pthread interceptors etc.

  align = (align < 8) ? 8 : align;
  void* allocated = [size, align]() {
    SpinMutexLock lock(&fallback_mutex);
    AllocatorCache* cache = &fallback_allocator_cache;
    return allocator.Allocate(cache, size, align);
  }();

  if (UNLIKELY(!allocated)) {
    SetAllocatorOutOfMemory();
    if (AllocatorMayReturnNull())
      return nullptr;
    ReportOutOfMemory(size, stack);
  }
  GetMetadata(allocated)->requested_size = size;
  ShadowUpdateMode mode = __copyprof_state.current_mode;
  if (mode != ShadowUpdateMode::CHECK) {
    MarkApplicationMemory(allocated, size, mode == ShadowUpdateMode::MARK_COPY);
  }
  return allocated;
}

void CopyProfDeallocate(void* ptr, uptr size, uptr align) {
  if (UNLIKELY(!ptr))
    return;
  if (__copyprof_state.current_mode == ShadowUpdateMode::CHECK) {
    __copyprof_state.is_transitive_copy &=
        IsMarkedAsCopy(ptr, GetMetadata(ptr)->requested_size);
  }
  SpinMutexLock lock(&fallback_mutex);
  AllocatorCache* cache = &fallback_allocator_cache;
  allocator.Deallocate(cache, ptr);
}

}  // namespace __copyprof
