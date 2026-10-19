// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#ifdef __SWITCH__
#include <memory>
#include <vector>
#endif

#include "Common/Assert.h"
#include "Common/MemArena.h"

namespace Common
{
// A bit set backed by a LazyMemoryRegion.
class LazyMemoryRegionBitSet final
{
public:
  // Allocates backing memory and creates a bit set.
  // Allocating backing memory may fail; use IsValid to check for success.
  LazyMemoryRegionBitSet(size_t bits);

  bool IsValid() const { return m_valid; }

  void Clear();

  bool TestBit(size_t bit) const
  {
    CheckBitIndexReadable(bit);
#ifdef __SWITCH__
    const u32* word = GetWord(bit, false);
    return word && (*word & (1u << (bit % 32))) != 0;
#else
    return (m_pointer[bit / 32] & (1 << (bit % 32))) != 0;
#endif
  }

  void SetBit(size_t bit)
  {
#ifdef __SWITCH__
    CheckBitIndexReadable(bit);
    *GetWord(bit, true) |= 1u << (bit % 32);
#else
    CheckBitIndexWriteable(bit);
    m_region.EnsureMemoryPageWritable(bit / 8);
    m_pointer[bit / 32] |= (1 << (bit % 32));
#endif
  }

  void ClearBit(size_t bit)
  {
#ifdef __SWITCH__
    CheckBitIndexReadable(bit);
    if (u32* word = GetWord(bit, false))
      *word &= ~(1u << (bit % 32));
#else
    CheckBitIndexWriteable(bit);
    m_region.EnsureMemoryPageWritable(bit / 8);
    m_pointer[bit / 32] &= ~(1 << (bit % 32));
#endif
  }

  void SetBits(size_t bits_start, size_t bits_end);

  void ClearBits(size_t bits_start, size_t bits_end);

#ifdef __SWITCH__
  const u32* GetRawMemory() const;
#else
  const u32* GetRawMemory() const { return m_pointer; }
#endif

private:
#ifdef __SWITCH__
  static constexpr size_t PAGE_BITS = 4096 * 8;
  mutable std::vector<std::unique_ptr<u32[]>> m_pages;
  mutable std::unique_ptr<u32[]> m_storage;
  mutable u32* m_pointer = nullptr;

  u32* GetWord(size_t bit, bool create) const
  {
    if (m_pointer)
      return m_pointer + bit / 32;
    auto& page = m_pages[bit / PAGE_BITS];
    if (!page && create)
      page = std::make_unique<u32[]>(PAGE_BITS / 32);
    return page ? page.get() + (bit % PAGE_BITS) / 32 : nullptr;
  }

  void UpdateBits(size_t bits_start, size_t bits_end, bool set);
#else
  LazyMemoryRegion m_region;
  u32* m_pointer;
#endif
  size_t m_bits;
  bool m_valid;

  bool Create(size_t bits);

  void CheckBitIndexReadable(size_t bit) const
  {
    DEBUG_ASSERT_MSG(COMMON, m_valid, "LazyMemoryRegionBitSet isn't valid");
    DEBUG_ASSERT_MSG(COMMON, bit < m_bits,
                     "Tried to access bit {} in a LazyMemoryRegionBitSet with only {} bits", bit,
                     m_bits);
  }

#ifndef __SWITCH__
  void CheckBitIndexWriteable(size_t bit)
  {
    CheckBitIndexReadable(bit);
    m_region.EnsureMemoryPageWritable(bit / 8);
  }

  void CheckBitIndicesWriteable(size_t bits_start, size_t bits_end);
#endif
};

}  // namespace Common
