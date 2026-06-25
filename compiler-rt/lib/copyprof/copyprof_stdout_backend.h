#ifndef COPYPROF_STDOUT_BACKEND_H_
#define COPYPROF_STDOUT_BACKEND_H_

#include "copyprof_report_backend.h"
#include "copyprof_report_buffer.h"

namespace __copyprof {

// Writes CopyProf reports to stdout.
class StdoutBackend : public ReportBackend {
 protected:
  ~StdoutBackend() = default;

 public:
  static StdoutBackend* Create();
  void Flush(ReportBuffer* buffer) override;
  void Finalize() override;
};

}  // namespace __copyprof

#endif  // COPYPROF_STDOUT_BACKEND_H_
