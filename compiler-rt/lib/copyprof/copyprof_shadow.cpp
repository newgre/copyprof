//===-- copyprof_shadow.cpp ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file implements the shadow memory management for CopyProf, tracking
/// the copy status and modification state of application memory.
///
//===----------------------------------------------------------------------===//

#include "copyprof_shadow.h"

namespace __copyprof {
namespace {

unsigned char BitMaskWithAllBitsSet(uptr num_bits) {
  CHECK(num_bits < sizeof(unsigned char) * 8 &&
        "Cannot fit more bits into unsigned char");
  return (1 << num_bits) - 1;
}

// Tracks whether a byte is marked as a copy.
// 1:8 mapping, i.e., one byte for every 8 bytes.
using CopyTrackingShadow = ShadowMemory<3>;
CopyTrackingShadow g_copy_shadow;

}  // namespace

void InitializeShadowMemory() {
  g_copy_shadow = CopyTrackingShadow::Create("copyprof");
}

void MarkApplicationMemory(const void* app_addr, uptr app_size, bool is_copy) {
  unsigned char* shadow_addr = reinterpret_cast<unsigned char*>(
      g_copy_shadow.MemToShadow(reinterpret_cast<uptr>(app_addr)));
  uptr shadow_size = CopyTrackingShadow::MemToShadowSize(app_size);
  internal_memset(shadow_addr, is_copy ? 0xFF : 0, shadow_size);
  uptr num_bits = app_size % 8;
  if (num_bits) {
    int start_bit = reinterpret_cast<uptr>(app_addr) % 8;
    if (is_copy) {
      shadow_addr[shadow_size] |= BitMaskWithAllBitsSet(num_bits) << start_bit;
    } else {
      shadow_addr[shadow_size] &= ~BitMaskWithAllBitsSet(num_bits) << start_bit;
    }
  }
}

bool IsMarkedAsCopy(const void* app_addr, uptr app_size) {
  unsigned char* shadow_addr = reinterpret_cast<unsigned char*>(
      g_copy_shadow.MemToShadow(reinterpret_cast<uptr>(app_addr)));
  uptr shadow_size = CopyTrackingShadow::MemToShadowSize(app_size);
  for (unsigned char *ptr = shadow_addr, *end_ptr = shadow_addr + shadow_size;
       ptr != end_ptr; ++ptr) {
    if (*ptr != 0xFF)
      return false;
  }
  uptr num_bits = app_size % 8;
  if (num_bits) {
    int start_bit = reinterpret_cast<uptr>(app_addr) % 8;
    return (shadow_addr[shadow_size] & BitMaskWithAllBitsSet(num_bits)
                                           << start_bit) ==
           BitMaskWithAllBitsSet(num_bits) << start_bit;
  }
  return true;
}

void DumpShadowMemory(const void* app_addr, uptr app_size) {
  const auto* shadow_ptr = reinterpret_cast<const unsigned char*>(
      g_copy_shadow.MemToShadow(reinterpret_cast<uptr>(app_addr)));
  Printf("Shadow memory for %p, size %zu: ", app_addr, app_size);
  for (uptr i = 0; i < CopyTrackingShadow::MemToShadowSize(app_size); ++i) {
    Printf("%x", shadow_ptr[i]);
  }
  uptr num_bits = app_size % 8;
  if (num_bits) {
    int start_bit = reinterpret_cast<uptr>(app_addr) % 8;
    Printf("%x", *shadow_ptr & BitMaskWithAllBitsSet(num_bits) << start_bit);
  }
  Printf("\n");
}

}  // namespace __copyprof
