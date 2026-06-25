#include "sanitizer_common/sanitizer_stacktrace.h"

namespace __sanitizer {

void BufferedStackTrace::UnwindImpl(uptr pc, uptr bp, void* context,
                                    bool request_fast, u32 max_depth) {
  Unwind(max_depth, pc, bp, context, 0, 0,
         StackTrace::WillUseFastUnwind(request_fast));
}

}  // namespace __sanitizer
