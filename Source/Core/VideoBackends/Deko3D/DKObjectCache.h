// Copyright 2026 Dolphin Emulator Project
// Copyright 2026 PalindromicBreadLoaf (palindromicbreadloaf@tuta.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>

#include <deko3d.hpp>

#include "Common/CommonTypes.h"

#include "VideoBackends/Deko3D/Constants.h"
#include "VideoCommon/RenderState.h"

namespace Deko3D
{
class DKStreamBuffer;

class DKObjectCache
{
public:
  ~DKObjectCache();

  static std::unique_ptr<DKObjectCache> Create();

  u32 GetSamplerIndex(const SamplerState& state);
  u32 GetPointSamplerIndex() const { return m_point_sampler_index; }

  DkGpuAddr GetSamplerDescriptorSetAddr() const { return m_descriptor_set_addr; }
  static constexpr u32 GetSamplerDescriptorCount() { return NUM_SAMPLER_DESCRIPTORS; }
  DKStreamBuffer* GetTextureUploadBuffer() const { return m_texture_upload_buffer.get(); }

  DkCmdBuf GetPipelineCaptureCommandBuffer() const { return m_pipeline_capture_cmdbuf; }
  std::mutex& GetPipelineCaptureMutex() { return m_pipeline_capture_mutex; }

  void ClearSamplerCache();

private:
  DKObjectCache() = default;
  bool Initialize();

  u32 AllocateSampler(const DkSampler& sampler);

  dk::UniqueMemBlock m_descriptor_block;
  DkSamplerDescriptor* m_descriptors = nullptr;
  DkGpuAddr m_descriptor_set_addr = DK_GPU_ADDR_INVALID;

  std::unique_ptr<DKStreamBuffer> m_texture_upload_buffer;

  dk::UniqueMemBlock m_pipeline_capture_memblock;
  dk::UniqueCmdBuf m_pipeline_capture_cmdbuf;
  std::mutex m_pipeline_capture_mutex;

  std::unordered_map<SamplerState, u32> m_sampler_cache;
  u32 m_next_index = 0;
  u32 m_point_sampler_index = 0;
};

extern std::unique_ptr<DKObjectCache> g_dk_object_cache;
}
