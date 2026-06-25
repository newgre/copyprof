#include "copyprof_stdout_backend.h"

#include "copyprof_report.h"
#include "sanitizer_common/sanitizer_allocator_internal.h"
#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_placement_new.h"
#include "sanitizer_common/sanitizer_stackdepot.h"

namespace __copyprof {

StdoutBackend* StdoutBackend::Create() {
  return new (InternalAlloc(sizeof(StdoutBackend))) StdoutBackend();
}

void StdoutBackend::Flush(ReportBuffer* buffer) {
  const char* data = buffer->data();
  usize pos = buffer->pos();
  InternalScopedString output;
  for (usize offset = 0; offset + sizeof(CopyProfReport) <= pos;
       offset += sizeof(CopyProfReport)) {
    output.clear();
    const auto* report = reinterpret_cast<const CopyProfReport*>(data + offset);
    output.AppendF(
        "[copyprof] Destroyed unnecessary copy amounting to %zu bytes:\n",
        report->copy_num_bytes);
    StackTrace trace = StackDepotGet(report->stack_trace_id);
    trace.PrintTo(&output);
    Printf("%s", output.data());
  }
}

void StdoutBackend::Finalize() {}

}  // namespace __copyprof
