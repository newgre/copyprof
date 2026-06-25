#include "copyprof_state.h"

namespace __copyprof {

THREADLOCAL PerThreadState __copyprof_state;

void PerObjectState::SetStaticSize(usize num_bytes) {
  tracked_size = num_bytes;
}

void PerObjectState::IncreaseTrackedSize(usize num_bytes) {
  tracked_size += num_bytes;
}

usize PerObjectState::GetTrackedSize() const { return tracked_size; }

}  // namespace __copyprof
