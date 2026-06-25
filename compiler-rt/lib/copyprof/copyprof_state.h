#ifndef COPYPROF_STATE_H
#define COPYPROF_STATE_H

#include "sanitizer_common/sanitizer_internal_defs.h"

namespace __copyprof {

// Determines how shadow memory is updated, specifically how stores and
// allocations affect shadow memory.
// The first time control flow reaches the entry point of a special member
// function, the current shadow update mode is changed accordingly.
// A copy c'tor or copy assignment operator sets mode to `MARK_COPY`, a c'tor
// to `MARK_NON_COPY`, and a d'tor to `CHECK`. In the `CHECK` mode, no shadow
// memory is updated at all. Once control flow leaves (any nested) special
// member functions, the state is set to `UNSPECIFIED`. In this mode, any stores
// will also mark shadow memory as "no copy".
enum class ShadowUpdateMode : u8 {
  UNSPECIFIED,
  MARK_NON_COPY,
  MARK_COPY,
  CHECK
};

// CopyProf uses per-thread state to figure out whether control flow is
// currently inside a special member function, and adapts updating of shadow
// memory accordingly (see ShadowUpdateMode). Since special member functions can
// nest arbitrarily, this state needs to be kept across function calls, so this
// state is stored in TLS. The nesting level counters are used to determine
// whether a top-level (i.e. the first in the call stack of special member
// functions) special member function has been reached.
struct PerThreadState {
  u32 construct_nesting_level = 0;
  u32 copy_nesting_level = 0;
  u32 destruct_nesting_level = 0;
  ShadowUpdateMode current_mode = ShadowUpdateMode::UNSPECIFIED;
  // Whether all transitively reachable d'tors from the top-level d'tor have
  // observed copies.
  bool is_transitive_copy = false;
  // When control flow enters a special member function, this is set to the
  // `this` pointer of the current object. This is used to look up the
  // per-object state outside of special member functions (e.g. when allocating
  // memory).
  const void* current_this_ptr = nullptr;
};

// Each copied object has an associated `PerObjectState`.
// Used for tracking the transitive size (size of self + all memory allocations)
// of the object.
struct PerObjectState {
  void SetStaticSize(usize num_bytes);
  void IncreaseTrackedSize(usize num_bytes);
  usize GetTrackedSize() const;

  // Whether memory was dynamically allocated while making the copy.
  bool did_allocate = false;

  // Total transitive size (self + all memory allocations) in bytes.
  usize tracked_size = 0;
};

extern THREADLOCAL PerThreadState __copyprof_state;

}  // namespace __copyprof

#endif  // COPYPROF_STATE_H
