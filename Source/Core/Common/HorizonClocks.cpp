// Copyright 2026 Dolphin Emulator Project
// Copyright 2026 PalindromicBreadLoaf (palindromicbreadloaf@tuta.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/HorizonClocks.h"

#include <mutex>

#include <switch.h>

namespace Common::HorizonClocks
{
namespace
{
std::mutex s_mutex;
unsigned int s_boost_count = 0;
bool s_boost_active = false;
}

void AcquireCpuBoost()
{
  if (appletGetAppletType() != AppletType_Application)
    return;

  std::lock_guard lock(s_mutex);
  if (s_boost_count++ != 0)
    return;

  s_boost_active = R_SUCCEEDED(appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad));
}

void ReleaseCpuBoost()
{
  if (appletGetAppletType() != AppletType_Application)
    return;

  std::lock_guard lock(s_mutex);
  if (s_boost_count == 0 || --s_boost_count != 0 || !s_boost_active)
    return;

  (void)appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
  s_boost_active = false;
}
}
