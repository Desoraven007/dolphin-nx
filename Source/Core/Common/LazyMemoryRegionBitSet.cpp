// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/LazyMemoryRegionBitSet.h"

#include <algorithm>
#include <cstring>

#include "Common/Align.h"
#include "Common/MsgHandler.h"

namespace Common
{
LazyMemoryRegionBitSet::LazyMemoryRegionBitSet(size_t bits) : m_bits(bits), m_valid(Create(bits))
{
}

bool LazyMemoryRegionBitSet::Create(size_t bits)
{
#ifdef __SWITCH__
  m_pages.resize(Common::DivideRoundingUp(bits, PAGE_BITS));
#else
  m_pointer = reinterpret_cast<u32*>(m_region.Create(Common::AlignUp(bits, 32) / 8));
  if (!m_pointer)
  {
    PanicAlertFmt("LazyMemoryRegionBitSet failed to create backing region");
    return false;
  }
#endif

  return true;
}

void LazyMemoryRegionBitSet::Clear()
{
#ifdef __SWITCH__
  if (m_pointer)
  {
    std::memset(m_pointer, 0, Common::DivideRoundingUp(m_bits, 32) * sizeof(u32));
  }
  else
  {
    for (auto& page : m_pages)
      page.reset();
  }
#else
  m_region.Clear();
#endif
}

#ifdef __SWITCH__
const u32* LazyMemoryRegionBitSet::GetRawMemory() const
{
  if (!m_pointer)
  {
    const size_t words = Common::DivideRoundingUp(m_bits, 32);
    m_storage = std::make_unique<u32[]>(words);
    for (size_t page = 0; page < m_pages.size(); ++page)
    {
      const size_t start = page * (PAGE_BITS / 32);
      if (m_pages[page])
        std::memcpy(m_storage.get() + start, m_pages[page].get(),
                    std::min(PAGE_BITS / 32, words - start) * sizeof(u32));
    }
    m_pages.clear();
    m_pointer = m_storage.get();
  }
  return m_pointer;
}

void LazyMemoryRegionBitSet::UpdateBits(size_t bits_start, size_t bits_end, bool set)
{
  if (bits_start >= bits_end)
    return;
  CheckBitIndexReadable(bits_end - 1);

  const auto update = [set](u32* memory, size_t start, size_t end) {
    const size_t first = start / 32;
    const size_t last = (end - 1) / 32;
    const u32 first_mask = 0xFFFFFFFFu << (start % 32);
    const u32 last_mask = 0xFFFFFFFFu >> (31 - (end - 1) % 32);
    if (first == last)
    {
      const u32 mask = first_mask & last_mask;
      memory[first] = set ? memory[first] | mask : memory[first] & ~mask;
      return;
    }
    memory[first] = set ? memory[first] | first_mask : memory[first] & ~first_mask;
    std::memset(memory + first + 1, set ? 0xFF : 0, (last - first - 1) * sizeof(u32));
    memory[last] = set ? memory[last] | last_mask : memory[last] & ~last_mask;
  };

  if (m_pointer)
  {
    update(m_pointer, bits_start, bits_end);
    return;
  }
  while (bits_start < bits_end)
  {
    const size_t page = bits_start / PAGE_BITS;
    const size_t start = bits_start % PAGE_BITS;
    const size_t end = std::min(bits_end - page * PAGE_BITS, PAGE_BITS);
    if (!m_pages[page] && set)
      m_pages[page] = std::make_unique<u32[]>(PAGE_BITS / 32);
    if (m_pages[page])
      update(m_pages[page].get(), start, end);
    bits_start = page * PAGE_BITS + end;
  }
}
#endif

#ifndef __SWITCH__
void LazyMemoryRegionBitSet::CheckBitIndicesWriteable(size_t bits_start, size_t bits_end)
{
  CheckBitIndexReadable(bits_end - 1);
  const size_t start_byte = Common::AlignDown(bits_start, 32) / 8;
  const size_t end_byte = Common::AlignUp(bits_end, 32) / 8;
  m_region.EnsureMemoryPagesWritable(start_byte, end_byte - start_byte);
}
#endif

void LazyMemoryRegionBitSet::SetBits(size_t bits_start, size_t bits_end)
{
#ifdef __SWITCH__
  UpdateBits(bits_start, bits_end, true);
#else
  if (bits_start >= bits_end)
    return;

  CheckBitIndicesWriteable(bits_start, bits_end);

  const u32 extra_first_word = 0xFFFFFFFF << bits_start % 32;
  const u32 extra_last_word = ~(0xFFFFFFFF << bits_end % 32);

  if (bits_start / 32 == bits_end / 32)
  {
    m_pointer[bits_start / 32] |= extra_first_word & extra_last_word;
    return;
  }

  if (bits_start % 32 != 0)
    m_pointer[bits_start / 32] |= extra_first_word;

  const size_t start_index = Common::DivideRoundingUp(bits_start, 32);
  const size_t end_index = bits_end / 32;
  std::memset(m_pointer + start_index, 0xFF, (end_index - start_index) * 4);

  if (bits_end % 32 != 0)
    m_pointer[bits_end / 32] |= extra_last_word;
#endif
}

void LazyMemoryRegionBitSet::ClearBits(size_t bits_start, size_t bits_end)
{
#ifdef __SWITCH__
  UpdateBits(bits_start, bits_end, false);
#else
  if (bits_start >= bits_end)
    return;

  CheckBitIndicesWriteable(bits_start, bits_end);

  const u32 extra_first_word = ~(0xFFFFFFFF << bits_start % 32);
  const u32 extra_last_word = 0xFFFFFFFF << bits_end % 32;

  if (bits_start / 32 == bits_end / 32)
  {
    m_pointer[bits_start / 32] &= extra_first_word | extra_last_word;
    return;
  }

  if (bits_start % 32 != 0)
    m_pointer[bits_start / 32] &= extra_first_word;

  const size_t start_index = Common::DivideRoundingUp(bits_start, 32);
  const size_t end_index = bits_end / 32;
  std::memset(m_pointer + start_index, 0x00, (end_index - start_index) * sizeof(u32));

  if (bits_end % 32 != 0)
    m_pointer[bits_end / 32] &= extra_last_word;
#endif
}

}  // namespace Common
