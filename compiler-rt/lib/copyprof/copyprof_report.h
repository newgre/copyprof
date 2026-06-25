#ifndef COPYPROF_REPORT_H_
#define COPYPROF_REPORT_H_

#include "copyprof_report_buffer.h"
#include "sanitizer_common/sanitizer_common.h"

namespace __copyprof {

// Describes an unnecessary copy made by the user application.
struct CopyProfReport {
  // The ID of the stack trace pointing to the location where the unnecessary
  // copy was destroyed.
  u32 stack_trace_id;
  // The total amount of the memory occupied by the unnecessary copy in bytes.
  usize copy_num_bytes;
};

// Ensures that any sequence of `CopyProfReport` instances are 8-byte aligned in
// memory. Each memory block starts with a `CopyProfReportBuffer` so it must
// adhere to the same size requirement. This avoids alignment UB when using
// typed pointers (e.g. to `CopyProfReport`) in buffer memory.
static_assert(sizeof(CopyProfReport) % 8 == 0);
static_assert(sizeof(ReportBuffer) % 8 == 0);

}  // namespace __copyprof

#endif  // COPYPROF_REPORT_H_
