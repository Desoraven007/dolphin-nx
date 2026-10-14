// Copyright 2026 Dolphin Emulator Project
// Copyright 2026 PalindromicBreadLoaf (palindromicbreadloaf@tuta.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>

#include <deko3d.hpp>

#include "Common/CommonTypes.h"

#include "VideoBackends/Deko3D/Constants.h"

namespace Deko3D
{
class DKFramebuffer;
class DKPipeline;
class DKShader;
class DKShaderCode;
class DKStreamBuffer;
class DKTexture;

class DKStateTracker
{
public:
  DKStateTracker();
  ~DKStateTracker();

  static DKStateTracker* GetInstance();
  static bool CreateInstance();
  static void DestroyInstance();

  DKFramebuffer* GetFramebuffer() const { return m_framebuffer; }
  const DKPipeline* GetPipeline() const { return m_pipeline; }

  void SetVertexBuffer(DkGpuAddr addr, u32 size);
  void SetIndexBuffer(DkGpuAddr addr, DkIdxFormat format);
  void SetFramebuffer(DKFramebuffer* framebuffer);
  void SetPipeline(const DKPipeline* pipeline);
  void SetComputeShader(const DKShader* shader);
  void SetGXUniformBuffer(u32 index, DkGpuAddr addr, u32 size);
  void SetUtilityUniformBuffer(DkGpuAddr addr, u32 size);
  void SetTexture(u32 index, const DKTexture* texture);
  void SetSampler(u32 index, u32 sampler_index);
  void SetSSBO(DkGpuAddr addr, u32 size);
  void SetImageTexture(u32 index, const DKTexture* texture);
  void SetTexelBuffer(u32 index, const DkImageDescriptor& descriptor);

  void UnbindTexture(const DKTexture* texture);

  void UnbindFramebuffer(const DKFramebuffer* framebuffer);

  void SetViewport(const DkViewport& viewport);
  void SetScissor(const DkScissor& scissor);

  void InvalidateCachedState();

  void BindFramebuffer();

  void ClearFramebuffer(const DkScissor& area, u32 color_mask, const std::array<float, 4>& color,
                        bool clear_depth, float depth);

  void DiscardFramebuffer();

  bool Bind();

  bool BindCompute();

private:
  enum DIRTY_FLAG : u32
  {
    DIRTY_FLAG_VERTEX_BUFFER = (1 << 0),
    DIRTY_FLAG_INDEX_BUFFER = (1 << 1),
    DIRTY_FLAG_PIPELINE = (1 << 2),
    DIRTY_FLAG_VIEWPORT = (1 << 3),
    DIRTY_FLAG_SCISSOR = (1 << 4),
    DIRTY_FLAG_FRAMEBUFFER = (1 << 5),
    DIRTY_FLAG_GX_UBOS = (1 << 6),
    DIRTY_FLAG_UTILITY_UBO = (1 << 7),
    DIRTY_FLAG_TEXTURES = (1 << 8),
    DIRTY_FLAG_SSBO = (1 << 9),
    DIRTY_FLAG_COMPUTE_SHADER = (1 << 10),
    DIRTY_FLAG_COMPUTE_IMAGES = (1 << 11),
    DIRTY_FLAG_STATIC_STATE = (1 << 12),

    DIRTY_FLAG_ALL = ~0u
  };

  bool Initialize();

  void BindStaticState(DkCmdBuf cmdbuf);

  bool PrepareTextureHandles();
  bool PrepareImageHandles();

  bool ReserveImageDescriptors(u32 count, u32* out_first_index);
  void CommitImageDescriptors(u32 count);
  DkImageDescriptor* GetImageDescriptors() const;

  void UpdateUniformBuffers(DkCmdBuf cmdbuf);

  u32 m_dirty_flags = DIRTY_FLAG_ALL;

  DkBufExtents m_vertex_buffer = {};
  DkGpuAddr m_index_buffer_addr = DK_GPU_ADDR_INVALID;
  DkIdxFormat m_index_format = DkIdxFormat_Uint16;

  const DKPipeline* m_pipeline = nullptr;
  const DKShader* m_compute_shader = nullptr;

  std::shared_ptr<DKShaderCode> m_compute_shader_code;

  DKFramebuffer* m_framebuffer = nullptr;

  std::array<DkBufExtents, NUM_UBO_BINDINGS> m_gx_ubos = {};
  DkBufExtents m_utility_ubo = {};
  DkBufExtents m_ssbo = {};

  std::array<const DKTexture*, NUM_PIXEL_SHADER_SAMPLERS> m_textures = {};
  std::array<u32, NUM_PIXEL_SHADER_SAMPLERS> m_samplers = {};
  std::array<const DKTexture*, NUM_COMPUTE_SHADER_SAMPLERS> m_image_textures = {};

  std::array<DkImageDescriptor, NUM_TEXEL_BUFFERS> m_texel_buffers = {};

  std::array<DkResHandle, NUM_FRAGMENT_TEXTURE_BINDINGS> m_texture_handles = {};
  std::array<DkResHandle, NUM_COMPUTE_SHADER_SAMPLERS> m_image_handles = {};

  DkViewport m_viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
  DkScissor m_scissor = {0, 0, 1, 1};

  std::unique_ptr<DKStreamBuffer> m_image_descriptors;

  dk::UniqueMemBlock m_dummy_buffer;
  std::unique_ptr<DKTexture> m_dummy_texture;
  std::unique_ptr<DKTexture> m_dummy_compute_texture;
  DkImageDescriptor m_dummy_texel_buffer = {};
};
}
