// Copyright 2026 Dolphin Emulator Project
// Copyright 2026 PalindromicBreadLoaf (palindromicbreadloaf@tuta.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include <deko3d.hpp>

#include "Common/CommonTypes.h"

#include "VideoCommon/AbstractPipeline.h"

namespace Deko3D
{
class DKShaderCode;

class DKPipeline final : public AbstractPipeline
{
public:
  DKPipeline(const AbstractPipelineConfig& config, std::vector<u32> replay_words,
             std::vector<std::shared_ptr<DKShaderCode>> shader_code, DkPrimitive primitive,
             u32 stage_mask);
  ~DKPipeline() override;

  void Replay(DkCmdBuf cmdbuf) const;

  bool IsValid() const { return !m_replay_words.empty(); }

  DkPrimitive GetPrimitive() const { return m_primitive; }
  AbstractPipelineUsage GetUsage() const { return m_config.usage; }
  u32 GetStageMask() const { return m_stage_mask; }

  static std::unique_ptr<DKPipeline> Create(const AbstractPipelineConfig& config);

private:
  std::vector<u32> m_replay_words;

  std::vector<std::shared_ptr<DKShaderCode>> m_shader_code;

  DkPrimitive m_primitive;
  u32 m_stage_mask;
};
}
