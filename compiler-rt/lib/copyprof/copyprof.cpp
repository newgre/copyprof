//===-- copyprof.cpp ------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file implements the core CopyProf runtime initialization and callback
/// functions inserted by the instrumentation passes.
///
//===----------------------------------------------------------------------===//

#include "copyprof_allocator.h"
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

// Whether CopyProf has been initialized.
bool copyprof_is_initialized;
// Whether CopyProf is currently initializing.
bool copyprof_init_is_running;

static void CheckUnwind() {
  UNINITIALIZED BufferedStackTrace trace;
  trace.Unwind(StackTrace::GetCurrentPc(), GET_CURRENT_FRAME(),
               /*context=*/nullptr, common_flags()->fast_unwind_on_check);
  trace.Print();
}

static ReportBackend* CreateReportBackend() {
  return StdoutBackend::Create();
}

static void Initialize() {
  if (LIKELY(copyprof_is_initialized))
    return;
  CHECK(!copyprof_init_is_running &&
        "BUG: CopyProf Initialize() must not call itself.");
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

static void MaybeUpdateCurrentMode(ShadowUpdateMode mode) {
  // Only the top level specifl member function changes the current mode. Until
  // control flow leaves the top level function, the mode logically remains the
  // same.
  if (__copyprof_state.current_mode == ShadowUpdateMode::UNSPECIFIED ||
      (__copyprof_state.construct_nesting_level == 0 &&
       __copyprof_state.copy_nesting_level == 0 &&
       __copyprof_state.destruct_nesting_level == 0)) {
    __copyprof_state.current_mode = mode;
  }
}

static void CopyMemberFunctionEnter(const void* this_ptr, uptr obj_size) {
  MaybeUpdateCurrentMode(ShadowUpdateMode::MARK_COPY);
  if (__copyprof_state.copy_nesting_level++ == 0) {
    // This is the top-level copy ctor so initialize per-object state.
    InitPerObjectState(this_ptr, obj_size);
    __copyprof_state.current_this_ptr = this_ptr;
  }
  MarkApplicationMemory(
      this_ptr, obj_size,
      __copyprof_state.current_mode == ShadowUpdateMode::MARK_COPY);
}

static void CopyMemberFunctionExit(const void* this_ptr, uptr obj_size) {
  --__copyprof_state.copy_nesting_level;
  MaybeUpdateCurrentMode(ShadowUpdateMode::UNSPECIFIED);
}

}  // namespace __copyprof

void __copyprof_init() { Initialize(); }

void __copyprof_ctor_enter_callback(const void* this_ptr, uptr obj_size) {
  MaybeUpdateCurrentMode(ShadowUpdateMode::MARK_NON_COPY);
  ++__copyprof_state.construct_nesting_level;
  MarkApplicationMemory(
      this_ptr, obj_size,
      __copyprof_state.current_mode == ShadowUpdateMode::MARK_COPY);
}

void __copyprof_ctor_exit_callback(const void* this_ptr, uptr obj_size) {
  --__copyprof_state.construct_nesting_level;
  MaybeUpdateCurrentMode(ShadowUpdateMode::UNSPECIFIED);
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
  MaybeUpdateCurrentMode(ShadowUpdateMode::CHECK);
  if (__copyprof_state.destruct_nesting_level++ == 0) {
    // Control flow just entered the top-level d'tor. Mark `is_transitive_copy`
    // as `true` assuming there are no nested d'tors (an individual d'tor is
    // transitively reachable from itself). If there are nested d'tors then any
    // of these may update this flag depending on whether they observe any
    // memory not marked as copy.
    __copyprof_state.is_transitive_copy = true;
  }
}

void __copyprof_dtor_exit_callback(const void* this_ptr, uptr obj_size) {
  --__copyprof_state.destruct_nesting_level;
  MaybeUpdateCurrentMode(ShadowUpdateMode::UNSPECIFIED);
  // If this is the top level-dtor and `this` is transitively marked as copy,
  // then an object has been found whose transitively owned memory is all marked
  // as copy, so a report is logged.
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
    // While executing in `CHECK` mode, do not update shadow memory at all to
    // avoid false negatives (objects may mutate their memory during destruction
    // but that shouldn't invalidate the identification as an unnecessary copy).
  } else {
    MarkApplicationMemory(
        addr, size,
        __copyprof_state.current_mode == ShadowUpdateMode::MARK_COPY);
  }
}
