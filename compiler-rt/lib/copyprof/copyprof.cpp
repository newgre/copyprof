#include "copyprof_allocator.h"
#include "copyprof_file_backend.h"
#include "copyprof_flags.h"
#include "copyprof_interceptors.h"
#include "copyprof_interface_internal.h"
#include "copyprof_per_object_state.h"
#include "copyprof_report_backend.h"
#include "copyprof_report_sink.h"
#include "copyprof_reporting.h"
#include "copyprof_shadow.h"
#include "copyprof_state.h"
#include "copyprof_stdout_backend.h"
#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_internal_defs.h"

using namespace __copyprof;

namespace __copyprof {

bool copyprof_is_initialized;
bool copyprof_init_is_running;

namespace {

void CheckUnwind() {
  UNINITIALIZED BufferedStackTrace trace;
  trace.Unwind(StackTrace::GetCurrentPc(), GET_CURRENT_FRAME(),
               /*context=*/nullptr, common_flags()->fast_unwind_on_check);
  trace.Print();
}

ReportBackend* CreateReportBackend() {
  ReportBackend* backend = FileBackend::Create(flags()->report_output_file);
  return backend != nullptr ? backend : StdoutBackend::Create();
}

void Initalize() {
  // This is thread-safe since this function is only called during
  // initialization.
  if (LIKELY(copyprof_is_initialized))
    return;
  CHECK(!copyprof_init_is_running && "CopyProf init calls itself!");
  copyprof_init_is_running = true;
  CacheBinaryName();
  InitializeFlags();
  // CopyProf uses interception so ensure we're not statically linked.
  __interception::DoesNotSupportStaticLinking();
  SetCheckUnwindCallback(&CheckUnwind);
  InitializePlatformEarly();
  InitializeInterceptors();
  InitializeShadowMemory();
  InitializePerObjectTracking();
  InitializeCopyProfAllocator();
  EnableBackgroundFlushing(CreateReportBackend());
  // Threads generally flush their report buffers when they exit or when their
  // buffers are full. If they keep running until after atexit handlers run,
  // those events will be lost.
  // The main thread executes the atexit handlers, so just before disabling the
  // background flusher, any remaining reports in the main thread's buffer are
  // flushed.
  Atexit([]() {
    FlushAndReturnCurrentThreadBuffer();
    DisableBackgroundFlushing();
  });
  copyprof_init_is_running = false;
  copyprof_is_initialized = true;
}

void UpdateCurrentMode(ShadowUpdateMode mode) {
  // Can only update mode on first entry to special member function. After that,
  // the mode remains "logically" the same so isn't updated subsequently.
  if (__copyprof_state.current_mode == ShadowUpdateMode::UNSPECIFIED ||
      (__copyprof_state.construct_nesting_level == 0 &&
       __copyprof_state.copy_nesting_level == 0 &&
       __copyprof_state.destruct_nesting_level == 0)) {
    __copyprof_state.current_mode = mode;
  }
}

void CopyMemberFunctionEnter(const void* this_ptr, uptr obj_size) {
  UpdateCurrentMode(ShadowUpdateMode::MARK_COPY);
  if (__copyprof_state.copy_nesting_level++ == 0) {
    // This is the top-level copy ctor so initialize per-object state.
    InitPerObjectState(this_ptr, obj_size);
    __copyprof_state.current_this_ptr = this_ptr;
  }
  MarkApplicationMemory(
      this_ptr, obj_size,
      __copyprof_state.current_mode == ShadowUpdateMode::MARK_COPY);
}

void CopyMemberFunctionExit(const void* this_ptr, uptr obj_size) {
  --__copyprof_state.copy_nesting_level;
  UpdateCurrentMode(ShadowUpdateMode::UNSPECIFIED);
}

}  // namespace
}  // namespace __copyprof

void __copyprof_init() { Initalize(); }

void __copyprof_ctor_enter_callback(const void* this_ptr, uptr obj_size) {
  UpdateCurrentMode(ShadowUpdateMode::MARK_NON_COPY);
  ++__copyprof_state.construct_nesting_level;
  MarkApplicationMemory(
      this_ptr, obj_size,
      __copyprof_state.current_mode == ShadowUpdateMode::MARK_COPY);
}

void __copyprof_ctor_exit_callback(const void* this_ptr, uptr obj_size) {
  --__copyprof_state.construct_nesting_level;
  UpdateCurrentMode(ShadowUpdateMode::UNSPECIFIED);
}

void __copyprof_copy_ctor_enter_callback(const void* this_ptr,
                                         const void* other_ptr, uptr obj_size) {
  CopyMemberFunctionEnter(this_ptr, obj_size);
}

void __copyprof_copy_ctor_exit_callback(const void* this_ptr,
                                        const void* other_ptr, uptr obj_size) {
  CopyMemberFunctionExit(this_ptr, obj_size);
}

void __copyprof_copy_assign_op_enter_callback(const void* this_ptr,
                                              const void* other_ptr,
                                              uptr obj_size) {
  CopyMemberFunctionEnter(this_ptr, obj_size);
}

void __copyprof_copy_assign_op_exit_callback(const void* this_ptr,
                                             const void* other_ptr,
                                             uptr obj_size) {
  CopyMemberFunctionExit(this_ptr, obj_size);
}

void __copyprof_dtor_enter_callback(const void* this_ptr, uptr obj_size) {
  UpdateCurrentMode(ShadowUpdateMode::CHECK);
  if (__copyprof_state.destruct_nesting_level++ == 0) {
    // This is the top-level d'tor. This may be the only involved d'tor so
    // assume that all transitively reachable d'tors have only seen copies. If
    // that turns out to be false, any of the transitive ones will set this to
    // `false`.
    __copyprof_state.is_transitive_copy = true;
  }
}

void __copyprof_dtor_exit_callback(const void* this_ptr, uptr obj_size) {
  --__copyprof_state.destruct_nesting_level;
  UpdateCurrentMode(ShadowUpdateMode::UNSPECIFIED);
  // If this is the top level-dtor and `this` including all sub-objects is
  // marked as copy, then a report is logged.
  __copyprof_state.is_transitive_copy &= IsMarkedAsCopy(this_ptr, obj_size);
  if (__copyprof_state.destruct_nesting_level == 0 &&
      __copyprof_state.is_transitive_copy) {
    LogCopyProfReport(GET_CALLER_PC(), GET_CURRENT_FRAME(),
                      GetPerObjectTrackedSize(this_ptr),
                      GetPerObjectDidAllocate(this_ptr));
  }
  // Clean up per-object state when top-level destructor exits.
  if (__copyprof_state.destruct_nesting_level == 0) {
    RemovePerObjectState(this_ptr);
  }
}

void __copyprof_store_callback(const void* addr, uptr size) {
  if (LIKELY(__copyprof_state.current_mode == ShadowUpdateMode::UNSPECIFIED)) {
    MarkApplicationMemory(addr, size, /*is_copy=*/false);
  } else if (__copyprof_state.current_mode == ShadowUpdateMode::CHECK) {
    // While executing in `CHECK` mode, do not update shadow memory at all.
  } else {
    MarkApplicationMemory(
        addr, size,
        __copyprof_state.current_mode == ShadowUpdateMode::MARK_COPY);
  }
}
