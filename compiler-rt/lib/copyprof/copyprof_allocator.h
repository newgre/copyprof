//===-- copyprof_allocator.h ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file declares the standalone memory allocator interface for CopyProf,
/// enabling isolated memory management for internal runtime structures.
///
//===----------------------------------------------------------------------===//

#ifndef COPYPROF_ALLOCATOR_H
#define COPYPROF_ALLOCATOR_H

#include "copyprof_internal.h"
#include "sanitizer_common/sanitizer_internal_defs.h"
#include "sanitizer_common/sanitizer_stacktrace.h"

namespace __copyprof {

// Initializes the copyprof allocator at startup.
void InitializeCopyProfAllocator();

// Allocates memory using the copyprof allocator.
void* CopyProfAllocate(uptr size, uptr align, BufferedStackTrace* stack);
void CopyProfDeallocate(void* ptr, uptr size, uptr align);

}  // namespace __copyprof

#endif  // COPYPROF_ALLOCATOR_H
