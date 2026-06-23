//===-- copyprof_reporting.h ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file declares the reporting interface for CopyProf, used to log
/// unnecessary copies identified during object destruction.
///
//===----------------------------------------------------------------------===//

#ifndef COPYPROF_REPORTING_H_
#define COPYPROF_REPORTING_H_

#include "sanitizer_common/sanitizer_common.h"

namespace __copyprof {

// Logs a CopyProf report to the console. `pc` and `bp` correspond to the
// program counter and frame pointer where the copy was destroyed. `obj_size`
// is the (flat) size in bytes of the destroyed object. `did_allocate`
// specifies whether memory was allocated when making the copy.
void LogCopyProfReport(uptr pc, uptr bp, uptr obj_size, bool did_allocate);

}  // namespace __copyprof

#endif  // COPYPROF_REPORTING_H_
