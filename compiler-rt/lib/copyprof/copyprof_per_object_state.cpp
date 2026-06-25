#include "copyprof_per_object_state.h"

#include "copyprof_state.h"
#include "sanitizer_common/sanitizer_dense_map.h"
#include "sanitizer_common/sanitizer_mutex.h"
#include "sanitizer_common/sanitizer_placement_new.h"

namespace __copyprof {
namespace {

// Global map for per-object size tracking.
// Protected by mutex for thread-safe access.
StaticSpinMutex g_per_obj_map_mutex;
DenseMap<const void*, PerObjectState> g_per_obj_map;

}  // namespace

void InitializePerObjectTracking() { g_per_obj_map_mutex.Init(); }

void InitPerObjectState(const void* this_ptr, uptr static_size) {
  SpinMutexLock lock(&g_per_obj_map_mutex);
  PerObjectState& state = g_per_obj_map[this_ptr];
  state.SetStaticSize(static_size);
  state.did_allocate = false;
}

void RecordPerObjectAllocation(const void* this_ptr, uptr num_bytes) {
  SpinMutexLock lock(&g_per_obj_map_mutex);
  PerObjectState& state = g_per_obj_map[this_ptr];
  state.IncreaseTrackedSize(num_bytes);
  state.did_allocate = true;
}

uptr GetPerObjectTrackedSize(const void* this_ptr) {
  SpinMutexLock lock(&g_per_obj_map_mutex);
  return g_per_obj_map[this_ptr].GetTrackedSize();
}

bool GetPerObjectDidAllocate(const void* this_ptr) {
  SpinMutexLock lock(&g_per_obj_map_mutex);
  return g_per_obj_map[this_ptr].did_allocate;
}

void RemovePerObjectState(const void* this_ptr) {
  SpinMutexLock lock(&g_per_obj_map_mutex);
  g_per_obj_map.erase(this_ptr);
}

}  // namespace __copyprof
