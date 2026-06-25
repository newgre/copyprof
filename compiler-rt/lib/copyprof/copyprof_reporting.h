#ifndef COPYPROF_REPORTING_H_
#define COPYPROF_REPORTING_H_

#include "sanitizer_common/sanitizer_common.h"

namespace __copyprof {

// Logs a CopyProf report to an in-memory buffer for the current thread. `pc`
// and `bp` correspond to the program counter and frame pointer where the copy
// was destroyed. `obj_size` is the (flat) size in bytes of the destroyed
// object. `did_allocate` specifies whether memory was allocated when making the
// copy.
void LogCopyProfReport(uptr pc, uptr bp, uptr obj_size, bool did_allocate);

// Must (only) be called from the main thread before background report flushing
// is disabled at process exit time. Needed because the main thread cannot rely
// on thread-local cleanup to trigger flushing (at that point the sink is
// already disabled).
void FlushAndReturnCurrentThreadBuffer();

}  // namespace __copyprof

#endif  // COPYPROF_REPORTING_H_