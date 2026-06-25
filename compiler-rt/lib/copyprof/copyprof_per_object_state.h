#ifndef COPYPROF_PER_OBJECT_STATE_H
#define COPYPROF_PER_OBJECT_STATE_H

#include "sanitizer_common/sanitizer_internal_defs.h"

namespace __copyprof {

// Initializes the per-object state tracking.
// Must be called once at program startup, after flags are initialized.
void InitializePerObjectTracking();

// Initializes the per-object state for the object at `this_ptr` with the given
// static size.
void InitPerObjectState(const void* this_ptr, uptr static_size);

// Records a dynamic allocation of `num_bytes` for the object at `this_ptr`.
void RecordPerObjectAllocation(const void* this_ptr, uptr num_bytes);

// Returns the tracked size for the object at `this_ptr`.
uptr GetPerObjectTrackedSize(const void* this_ptr);

// Returns whether the object at `this_ptr` has performed any dynamic
// allocations.
bool GetPerObjectDidAllocate(const void* this_ptr);

// Removes the per-object state for the object at `this_ptr`.
void RemovePerObjectState(const void* this_ptr);

}  // namespace __copyprof

#endif  // COPYPROF_PER_OBJECT_STATE_H
