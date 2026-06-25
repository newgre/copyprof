#ifndef COPYPROF_REPORT_SINK_H_
#define COPYPROF_REPORT_SINK_H_

#include "copyprof_report_backend.h"
#include "copyprof_report_buffer.h"

namespace __copyprof {

// Obtains a new memory buffer. Thread-safe.
ReportBuffer* AcquireReportBuffer();
// Returns a `ReportBuffer` to the sink so it can be flushed eventually.
// Thread-safe.
void ReturnReportBuffer(ReportBuffer* buffer);

// Must be called by the main thread at initialization time to enable flushing
// of reports by a background thread. Must be preceded by a call to
// `DisableBackgroundFlushing` before it can be called again.
// Takes ownership of the given pointer. The memory is freed in
// `DisableBackgroundFlushing`.
void EnableBackgroundFlushing(ReportBackend* backend);

// Must be called by the main thread typically at program exit, but can be
// safely called arbitrarily often. All report buffers that have previously been
// returned to the sink are flushed (if any). Any report buffers still owned by
// threads (that are still alive and did not return their buffers) are not
// flushed, and such reports will be lost. Also closes the report output file
// if one was opened.
void DisableBackgroundFlushing();

}  // namespace __copyprof

#endif  // COPYPROF_REPORT_SINK_H_
