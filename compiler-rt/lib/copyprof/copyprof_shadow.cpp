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

// Tracks whether application memory is marked as a copy.
ShadowMemory g_copy_shadow;

// Returns a mask with `num_bits` bits set, starting at `start_bit`. Both are
// relative to a single shadow byte, so the range must fit within one.
unsigned char BitMask(uptr start_bit, uptr num_bits) {
  CHECK_LE(start_bit + num_bits, kBytesPerShadowByte);
  return static_cast<unsigned char>(((1u << num_bits) - 1u) << start_bit);
}

// Sets (or clears) `num_bits` bits of `*shadow_byte` starting at `start_bit`.
void UpdateBits(unsigned char* shadow_byte, uptr start_bit, uptr num_bits,
                bool is_copy) {
  const unsigned char mask = BitMask(start_bit, num_bits);
  if (is_copy)
    *shadow_byte |= mask;
  else
    *shadow_byte &= static_cast<unsigned char>(~mask);
}

// Whether all `num_bits` bits of `shadow_byte` starting at `start_bit` are set.
bool AllBitsSet(unsigned char shadow_byte, uptr start_bit, uptr num_bits) {
  const unsigned char mask = BitMask(start_bit, num_bits);
  return (shadow_byte & mask) == mask;
}

// Number of bits an application range of `num_bytes` bytes starting at bit
// offset `start_bit` occupies in its first shadow byte.
uptr HeadBits(uptr start_bit, uptr num_bytes) {
  return Min(num_bytes, kBytesPerShadowByte - start_bit);
}

}  // namespace

void InitializeShadowMemory() {
  g_copy_shadow = ShadowMemory::Create("copyprof");
}

void MarkApplicationMemory(const void* app_addr, uptr num_bytes, bool is_copy) {
  CHECK_GT(num_bytes, 0);
  const uptr addr = reinterpret_cast<uptr>(app_addr);
  unsigned char* shadow =
      reinterpret_cast<unsigned char*>(g_copy_shadow.MemToShadow(addr));
  // An application range need not start on a shadow byte boundary, so it is
  // updated in three steps: the leading (possibly partial) shadow byte, the
  // whole shadow bytes in the middle, and the trailing partial byte. Updating
  // whole bytes without accounting for `start_bit` would corrupt the bits of
  // the neighbouring objects that share the first and last shadow byte.
  const uptr start_bit = addr % kBytesPerShadowByte;
  const uptr head_bits = HeadBits(start_bit, num_bytes);
  UpdateBits(shadow, start_bit, head_bits, is_copy);
  ++shadow;

  uptr remaining = num_bytes - head_bits;
  const uptr full_bytes = remaining / kBytesPerShadowByte;
  if (full_bytes > 0) {
    internal_memset(shadow, is_copy ? 0xFF : 0, full_bytes);
    shadow += full_bytes;
    remaining -= full_bytes * kBytesPerShadowByte;
  }
  if (remaining > 0)
    UpdateBits(shadow, /*start_bit=*/0, remaining, is_copy);
}

bool IsMarkedAsCopy(const void* app_addr, uptr num_bytes) {
  CHECK_GT(num_bytes, 0);
  const uptr addr = reinterpret_cast<uptr>(app_addr);
  const auto* shadow =
      reinterpret_cast<const unsigned char*>(g_copy_shadow.MemToShadow(addr));
  // Mirrors the three-step traversal of `MarkApplicationMemory`.
  const uptr start_bit = addr % kBytesPerShadowByte;
  const uptr head_bits = HeadBits(start_bit, num_bytes);
  if (!AllBitsSet(*shadow, start_bit, head_bits))
    return false;
  ++shadow;

  uptr remaining = num_bytes - head_bits;
  const uptr full_bytes = remaining / kBytesPerShadowByte;
  for (uptr i = 0; i < full_bytes; ++i) {
    if (shadow[i] != 0xFF)
      return false;
  }
  shadow += full_bytes;
  remaining -= full_bytes * kBytesPerShadowByte;
  return remaining == 0 || AllBitsSet(*shadow, /*start_bit=*/0, remaining);
}

void DumpShadowMemory(const void* app_addr, uptr num_bytes) {
  const uptr addr = reinterpret_cast<uptr>(app_addr);
  const auto* shadow =
      reinterpret_cast<const unsigned char*>(g_copy_shadow.MemToShadow(addr));
  // Every shadow byte the range touches, including the partial ones at either
  // end.
  const uptr num_shadow_bytes =
      (addr % kBytesPerShadowByte + num_bytes + kBytesPerShadowByte - 1) /
      kBytesPerShadowByte;
  Printf("Shadow memory for %p, size %zu: ", app_addr, num_bytes);
  for (uptr i = 0; i < num_shadow_bytes; ++i) {
    Printf("%02x", shadow[i]);
  }
  Printf("\n");
}

}  // namespace __copyprof
