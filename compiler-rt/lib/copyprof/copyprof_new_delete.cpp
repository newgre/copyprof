// Routines to intercept C and C++ (de-)allocation functions.

#include <stddef.h>

#include "copyprof_allocator.h"
#include "copyprof_per_object_state.h"
#include "copyprof_shadow.h"
#include "copyprof_state.h"
#include "interception/interception.h"
#include "sanitizer_common/sanitizer_allocator.h"
#include "sanitizer_common/sanitizer_allocator_report.h"
#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_internal_defs.h"
#include "sanitizer_common/sanitizer_stacktrace.h"

// Fake std::nothrow_t and std::align_val_t to avoid including <new>.
namespace std {
struct nothrow_t {};
enum class align_val_t : size_t {};
}  // namespace std

using namespace __copyprof;

ALWAYS_INLINE
void* OperatorNew(size_t size, std::align_val_t align, bool no_throw) {
  // TODO(jannewger): implement proper alignment checking.
  UNINITIALIZED BufferedStackTrace stack;
  void* result = CopyProfAllocate(size, static_cast<uptr>(align), &stack);
  if (!no_throw && UNLIKELY(!result)) {
    BufferedStackTrace stack;
    stack.top_frame_bp = GET_CURRENT_FRAME();
    stack.trace_buffer[0] = StackTrace::GetCurrentPc();
    ReportOutOfMemory(size, &stack);
  }
  if (__copyprof_state.current_mode == ShadowUpdateMode::MARK_COPY) {
    RecordPerObjectAllocation(__copyprof_state.current_this_ptr, size);
  }
  return result;
}

ALWAYS_INLINE
void OperatorDelete(void* ptr, size_t size, std::align_val_t align) {
  CopyProfDeallocate(ptr, size, static_cast<uptr>(align));
}

INTERCEPTOR_ATTRIBUTE
void* operator new(size_t size) {
  return OperatorNew(size, std::align_val_t{0},
                     /*no_throw=*/false);
}
INTERCEPTOR_ATTRIBUTE
void* operator new(size_t size, std::align_val_t align) {
  return OperatorNew(size, align, /*no_throw=*/false);
}
INTERCEPTOR_ATTRIBUTE
void* operator new(size_t size, std::nothrow_t const&) {
  return OperatorNew(size, std::align_val_t{0},
                     /*no_throw=*/true);
}
INTERCEPTOR_ATTRIBUTE
void* operator new(size_t size, std::align_val_t align, std::nothrow_t const&) {
  return OperatorNew(size, align, /*no_throw=*/true);
}
INTERCEPTOR_ATTRIBUTE
void* operator new[](size_t size) {
  return OperatorNew(size, std::align_val_t{0},
                     /*no_throw=*/false);
}
INTERCEPTOR_ATTRIBUTE
void* operator new[](size_t size, std::align_val_t align) {
  return OperatorNew(size, align, /*no_throw=*/false);
}
INTERCEPTOR_ATTRIBUTE
void* operator new[](size_t size, std::nothrow_t const&) {
  return OperatorNew(size, std::align_val_t{0},
                     /*no_throw=*/true);
}
INTERCEPTOR_ATTRIBUTE
void* operator new[](size_t size, std::align_val_t align,
                     std::nothrow_t const&) {
  return OperatorNew(size, align, /*no_throw=*/true);
}

INTERCEPTOR_ATTRIBUTE
void operator delete(void* ptr) NOEXCEPT {
  return OperatorDelete(ptr, /*size=*/0, std::align_val_t{0});
}
INTERCEPTOR_ATTRIBUTE
void operator delete(void* ptr, size_t size) NOEXCEPT {
  return OperatorDelete(ptr, size, std::align_val_t{0});
}
INTERCEPTOR_ATTRIBUTE
void operator delete(void* ptr, std::align_val_t align) NOEXCEPT {
  return OperatorDelete(ptr, /*size=*/0, align);
}
INTERCEPTOR_ATTRIBUTE
void operator delete(void* ptr, std::nothrow_t const&) {
  return OperatorDelete(ptr, /*size=*/0, std::align_val_t{0});
}
INTERCEPTOR_ATTRIBUTE
void operator delete(void* ptr, std::align_val_t align, std::nothrow_t const&) {
  return OperatorDelete(ptr, /*size=*/0, align);
}
INTERCEPTOR_ATTRIBUTE
void operator delete(void* ptr, size_t size, std::align_val_t align) NOEXCEPT {
  return OperatorDelete(ptr, size, align);
}
INTERCEPTOR_ATTRIBUTE
void operator delete[](void* ptr) NOEXCEPT {
  return OperatorDelete(ptr, /*size=*/0, std::align_val_t{0});
}
INTERCEPTOR_ATTRIBUTE
void operator delete[](void* ptr, size_t size) NOEXCEPT {
  return OperatorDelete(ptr, size, std::align_val_t{0});
}
INTERCEPTOR_ATTRIBUTE
void operator delete[](void* ptr, std::align_val_t align) NOEXCEPT {
  return OperatorDelete(ptr, /*size=*/0, align);
}
INTERCEPTOR_ATTRIBUTE
void operator delete[](void* ptr, std::nothrow_t const&) {
  return OperatorDelete(ptr, /*size=*/0, std::align_val_t{0});
}
INTERCEPTOR_ATTRIBUTE
void operator delete[](void* ptr, std::align_val_t align,
                       std::nothrow_t const&) {
  return OperatorDelete(ptr, /*size=*/0, align);
}
INTERCEPTOR_ATTRIBUTE
void operator delete[](void* ptr, size_t size,
                       std::align_val_t align) NOEXCEPT {
  return OperatorDelete(ptr, size, align);
}
