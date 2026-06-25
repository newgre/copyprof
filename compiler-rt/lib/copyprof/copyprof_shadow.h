#ifndef COPYPROF_SHADOW_H
#define COPYPROF_SHADOW_H

#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_internal_defs.h"

namespace __copyprof {

// Helper template for mapping application addresses to shadow memory.
// `ShadowScale` determines the compression ratio (1 shadow byte per
// 2^ShadowScale application bytes).
template <uptr ShadowScale>
struct ShadowMemory {
  static uptr MemToShadowSize(uptr size) { return size >> ShadowScale; }
  static ShadowMemory<ShadowScale> Create(const char* name) {
    uptr max_user_va = GetMaxUserVirtualAddress();
    uptr shadow_size_bytes = MemToShadowSize(max_user_va);
    uptr mapped = MapDynamicShadow(shadow_size_bytes, ShadowScale,
                                   /*min_shadow_base_alignment=*/0, max_user_va,
                                   GetMmapGranularity());
    ReserveShadowMemoryRange(mapped, mapped + shadow_size_bytes, name,
                             /*madvise_shadow=*/true);
    return ShadowMemory<ShadowScale>(mapped);
  }
  ShadowMemory() = default;
  uptr MemToShadow(uptr p) const { return (p >> ShadowScale) + shadow_base_; }

 private:
  explicit ShadowMemory(uptr shadow_base) : shadow_base_(shadow_base) {}
  uptr shadow_base_ = 0;
};

// Must be called exactly once at program startup.
void InitializeShadowMemory();

// Given an application memory block starting at `app_addr` of size `app_size`,
// marks the corresponding shadow memory as a copy or non-copy.
void MarkApplicationMemory(const void* app_addr, uptr app_size, bool is_copy);

// Whether the application memory block starting at `app_addr` of size
// `app_size` is marked as a copy in shadow memory.
bool IsMarkedAsCopy(const void* app_addr, uptr app_size);

// Dumps shadow memory contents to stdout. Useful for debugging.
void DumpShadowMemory(const void* app_addr, uptr app_size);

}  // namespace __copyprof

#endif  // COPYPROF_SHADOW_H
