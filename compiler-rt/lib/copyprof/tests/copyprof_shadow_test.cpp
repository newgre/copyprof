//===-- copyprof_shadow_test.cpp -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "copyprof/copyprof_shadow.h"
#include "copyprof/copyprof_interface_internal.h"
#include "gtest/gtest.h"

namespace __copyprof {
namespace {

TEST(CopyProfShadowTest, AlignedMemory) {
  __copyprof_init();
  u64 buf[4] = {0};
  MarkApplicationMemory(buf, sizeof(buf), /*is_copy=*/true);
  EXPECT_TRUE(IsMarkedAsCopy(buf, sizeof(buf)));
  MarkApplicationMemory(buf, sizeof(buf), /*is_copy=*/false);
  EXPECT_FALSE(IsMarkedAsCopy(buf, sizeof(buf)));
}

TEST(CopyProfShadowTest, UnalignedMemory) {
  __copyprof_init();
  unsigned char buf[64] = {0};
  MarkApplicationMemory(buf, sizeof(buf), /*is_copy=*/false);

  // Mark an unaligned slice in the middle as a copy.
  MarkApplicationMemory(buf + 3, 5, /*is_copy=*/true);
  EXPECT_TRUE(IsMarkedAsCopy(buf + 3, 5));

  // Ensure surrounding unassigned bytes remain marked as non-copy.
  EXPECT_FALSE(IsMarkedAsCopy(buf, 3));
  EXPECT_FALSE(IsMarkedAsCopy(buf + 8, 8));
}

// An unaligned range that spans more than one shadow byte. Updating whole
// shadow bytes without accounting for the start bit corrupts the bits of the
// object sharing the leading shadow byte, and never marks the bits past it.
// Bytes are queried one at a time on purpose: a range query would share any
// masking bug with the update path and hide the defect.
TEST(CopyProfShadowTest, UnalignedRangeSpanningShadowBytes) {
  __copyprof_init();
  alignas(8) unsigned char buf[32] = {0};
  MarkApplicationMemory(buf, sizeof(buf), /*is_copy=*/false);

  // Eight bytes at offset 4: bits 4-7 of the first shadow byte and bits 0-3 of
  // the second.
  MarkApplicationMemory(buf + 4, 8, /*is_copy=*/true);

  for (uptr i = 0; i < 4; ++i)
    EXPECT_FALSE(IsMarkedAsCopy(buf + i, 1)) << "byte " << i;
  for (uptr i = 4; i < 12; ++i)
    EXPECT_TRUE(IsMarkedAsCopy(buf + i, 1)) << "byte " << i;
  for (uptr i = 12; i < 16; ++i)
    EXPECT_FALSE(IsMarkedAsCopy(buf + i, 1)) << "byte " << i;
}

// A range whose bits straddle a shadow byte boundary, i.e. where
// `start_bit + num_bits` exceeds the bits in one shadow byte. The overflowing
// bits belong to the next shadow byte and must not be truncated away.
TEST(CopyProfShadowTest, UnalignedRangeCrossingByteBoundary) {
  __copyprof_init();
  alignas(8) unsigned char buf[32] = {0};
  MarkApplicationMemory(buf, sizeof(buf), /*is_copy=*/false);

  // Four bytes at offset 6: bits 6-7 of the first shadow byte and bits 0-1 of
  // the second.
  MarkApplicationMemory(buf + 6, 4, /*is_copy=*/true);

  for (uptr i = 0; i < 6; ++i)
    EXPECT_FALSE(IsMarkedAsCopy(buf + i, 1)) << "byte " << i;
  for (uptr i = 6; i < 10; ++i)
    EXPECT_TRUE(IsMarkedAsCopy(buf + i, 1)) << "byte " << i;
  for (uptr i = 10; i < 16; ++i)
    EXPECT_FALSE(IsMarkedAsCopy(buf + i, 1)) << "byte " << i;
}

// The same geometry in the clearing direction: a store to an unaligned field
// must not clear the copy bits of the bytes around it.
TEST(CopyProfShadowTest, ClearingUnalignedRangeKeepsNeighbours) {
  __copyprof_init();
  alignas(8) unsigned char buf[32] = {0};
  MarkApplicationMemory(buf, sizeof(buf), /*is_copy=*/true);

  MarkApplicationMemory(buf + 6, 4, /*is_copy=*/false);

  for (uptr i = 0; i < 6; ++i)
    EXPECT_TRUE(IsMarkedAsCopy(buf + i, 1)) << "byte " << i;
  for (uptr i = 6; i < 10; ++i)
    EXPECT_FALSE(IsMarkedAsCopy(buf + i, 1)) << "byte " << i;
  for (uptr i = 10; i < 16; ++i)
    EXPECT_TRUE(IsMarkedAsCopy(buf + i, 1)) << "byte " << i;
}

TEST(CopyProfShadowTest, PartialOverwrite) {
  __copyprof_init();
  u64 buf[4] = {0};
  MarkApplicationMemory(buf, sizeof(buf), /*is_copy=*/true);
  EXPECT_TRUE(IsMarkedAsCopy(buf, sizeof(buf)));

  // Simulate modifying a sub-object or field in the middle of the buffer.
  MarkApplicationMemory(&buf[1], sizeof(u64), /*is_copy=*/false);
  EXPECT_FALSE(IsMarkedAsCopy(buf, sizeof(buf)));

  // Untouched surrounding memory should still be marked as copy.
  EXPECT_TRUE(IsMarkedAsCopy(&buf[0], sizeof(u64)));
  EXPECT_TRUE(IsMarkedAsCopy(&buf[2], 2 * sizeof(u64)));
}

}  // namespace
}  // namespace __copyprof
