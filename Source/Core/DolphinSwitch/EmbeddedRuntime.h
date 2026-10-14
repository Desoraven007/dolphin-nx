// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "DolphinSwitch/Launcher.h"

namespace DolphinSwitch::EmbeddedRuntime
{
constexpr std::string_view RETURN_ARGUMENT = "--dolphin-runtime-return";
constexpr std::string_view LAUNCH_ARGUMENT = "--dolphin-runtime-launch";

struct Request
{
  std::string launcher_path;
  std::optional<LaunchRequest> game;
};

using ExtractProgress = std::function<void(std::uint64_t done, std::uint64_t total)>;

bool IsCached();
bool Launch(const std::string& launcher_path, const LaunchRequest& game, std::string* error,
            const ExtractProgress& progress = {});
std::optional<Request> ReadRequest();
bool ReturnToLauncher(const std::string& launcher_path, const std::string& message);
std::string ReadReturnMessage();

}
