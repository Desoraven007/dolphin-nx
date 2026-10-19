// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinSwitch/ScreenLayout.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <locale>
#include <mutex>
#include <sstream>
#include <string_view>

#include <imgui.h>
#include <switch.h>

#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "InputCommon/ControllerInterface/Switch/Switch.h"

namespace DolphinSwitch::ScreenLayout
{
namespace
{
constexpr std::size_t SCREEN_COUNT = 5;

struct Placement
{
  int x = 0;
  int y = 0;
  int width = 0;
  bool operator==(const Placement&) const = default;
};

using Layout = std::array<Placement, SCREEN_COUNT>;

Layout Parse(std::string_view text)
{
  Layout layout{};
  if (text.empty())
    return layout;
  std::istringstream stream{std::string(text)};
  stream.imbue(std::locale::classic());
  int version;
  if (!(stream >> version) || version != 1)
    return {};
  for (Placement& placement : layout)
  {
    if (!(stream >> placement.x >> placement.y >> placement.width) || placement.x < 0 ||
        placement.x > 10000 || placement.y < 0 || placement.y > 10000 || placement.width < 0 ||
        placement.width > 10000)
    {
      return {};
    }
  }
  stream >> std::ws;
  return stream.eof() ? layout : Layout{};
}

std::string Serialize(const Layout& layout)
{
  if (layout == Layout{})
    return {};
  std::string text = "1";
  for (const Placement& placement : layout)
  {
    text += " " + std::to_string(placement.x) + " " + std::to_string(placement.y) + " " +
            std::to_string(placement.width);
  }
  return text;
}

MathUtil::Rectangle<float> Resolve(Placement placement, float aspect, float width, float height)
{
  if (!(width > 0 && height > 0 && aspect > 0) || !std::isfinite(aspect))
    return {};
  const float maximum = std::min(width, height * aspect);
  const float screen_width =
      std::clamp(width * placement.width / 10000.0f, maximum * 0.05f, maximum);
  const float screen_height = screen_width / aspect;
  const float x =
      std::clamp(width * placement.x / 10000.0f, 0.0f, std::max(0.0f, width - screen_width));
  const float y =
      std::clamp(height * placement.y / 10000.0f, 0.0f, std::max(0.0f, height - screen_height));
  return {x, y, x + screen_width, y + screen_height};
}

using Rectangle = MathUtil::Rectangle<float>;
std::mutex s_mutex;
Layout s_layout{};
Layout s_before_edit{};
std::array<Rectangle, SCREEN_COUNT> s_defaults{};
std::array<Rectangle, SCREEN_COUNT> s_rectangles{};
std::array<bool, SCREEN_COUNT> s_available{};
std::string s_config;
std::string s_game_ini;
std::string s_error;
float s_width = 1280;
float s_height = 720;
bool s_editing = false;
std::size_t s_selected = 0;
bool s_touch_down = false;
bool s_resizing = false;
float s_touch_x = 0;
float s_touch_y = 0;
Rectangle s_touch_start{};
std::chrono::steady_clock::time_point s_last_input{};

Rectangle Apply(std::size_t index, const Rectangle& rectangle, float width, float height)
{
  if (width <= 0 || height <= 0 || rectangle.GetWidth() <= 0 || rectangle.GetHeight() <= 0)
    return rectangle;
  std::lock_guard lock(s_mutex);
  if (!s_editing)
  {
    const std::string config = Config::Get(Config::GFX_SWITCH_SCREEN_LAYOUT);
    if (config != s_config)
    {
      s_config = config;
      s_layout = Parse(config);
    }
  }
  s_width = width;
  s_height = height;
  s_defaults[index] = rectangle;
  s_rectangles[index] =
      s_layout[index].width == 0 ?
          rectangle :
          Resolve(s_layout[index], rectangle.GetWidth() / rectangle.GetHeight(), width, height);
  return s_rectangles[index];
}

void SetRectangle(Rectangle rectangle, bool snap_to_edges = false)
{
  const float aspect = s_defaults[s_selected].GetWidth() / s_defaults[s_selected].GetHeight();
  Placement placement{static_cast<int>(std::lround(rectangle.left * 10000 / s_width)),
                      static_cast<int>(std::lround(rectangle.top * 10000 / s_height)),
                      static_cast<int>(std::lround(
                          std::max(0.0f, rectangle.right - rectangle.left) * 10000 / s_width))};
  rectangle = Resolve(placement, aspect, s_width, s_height);
  const float snap = snap_to_edges ? 6.0f * s_height / 720 : 0.0f;
  float x = rectangle.left;
  float y = rectangle.top;
  if (x < snap)
    x = 0;
  else if (s_width - rectangle.right < snap)
    x = s_width - rectangle.GetWidth();
  if (y < snap)
    y = 0;
  else if (s_height - rectangle.bottom < snap)
    y = s_height - rectangle.GetHeight();
  s_layout[s_selected] = {static_cast<int>(std::lround(x * 10000 / s_width)),
                          static_cast<int>(std::lround(y * 10000 / s_height)),
                          static_cast<int>(std::lround(rectangle.GetWidth() * 10000 / s_width))};
  s_rectangles[s_selected] = Resolve(s_layout[s_selected], aspect, s_width, s_height);
}

bool Save()
{
  Common::IniFile ini;
  if (s_game_ini.empty() || (File::Exists(s_game_ini) && !ini.Load(s_game_ini)) ||
      !File::CreateFullPath(s_game_ini))
    return false;
  const std::string value = Serialize(s_layout);
  ini.GetOrCreateSection("Video_Settings")->Set("SwitchScreenLayout", value);
  if (!ini.Save(s_game_ini))
    return false;
  Config::Set(Config::LayerType::LocalGame, Config::GFX_SWITCH_SCREEN_LAYOUT, value);
  s_config = value;
  s_editing = false;
  return true;
}
}  // namespace

MathUtil::Rectangle<int> ApplyMain(const MathUtil::Rectangle<int>& rectangle, int width, int height)
{
  const Rectangle result =
      Apply(0,
            {static_cast<float>(rectangle.left), static_cast<float>(rectangle.top),
             static_cast<float>(rectangle.right), static_cast<float>(rectangle.bottom)},
            width, height);
  return {static_cast<int>(std::lround(result.left)), static_cast<int>(std::lround(result.top)),
          static_cast<int>(std::lround(result.right)),
          static_cast<int>(std::lround(result.bottom))};
}

Rectangle ApplyGBA(std::size_t port, const Rectangle& rectangle, float width, float height)
{
  return port < 4 ? Apply(port + 1, rectangle, width, height) : rectangle;
}

void BeginSession(std::string game_id, unsigned revision)
{
  std::lock_guard lock(s_mutex);
  s_editing = false;
  s_game_ini = ConfigLoaders::GetLocalGameConfigOverridePath();
  if (s_game_ini.empty() && !game_id.empty())
  {
    const std::string base = File::GetUserPath(D_GAMESETTINGS_IDX) + game_id;
    const std::string specific = base + "r" + std::to_string(revision) + ".ini";
    s_game_ini = File::Exists(specific) ? specific : base + ".ini";
  }
  s_config = Config::Get(Config::GFX_SWITCH_SCREEN_LAYOUT);
  s_layout = Parse(s_config);
}

void EndSession()
{
  std::lock_guard lock(s_mutex);
  s_editing = false;
  s_layout = {};
  s_config.clear();
  s_game_ini.clear();
  s_defaults = {};
  s_rectangles = {};
}

void BeginEdit(const std::array<bool, 4>& gba_ports)
{
  std::lock_guard lock(s_mutex);
  s_before_edit = s_layout;
  s_available[0] = true;
  for (std::size_t port = 0; port < gba_ports.size(); ++port)
    s_available[port + 1] =
        gba_ports[port] &&
        Config::Get(Config::MAIN_SWITCH_GBA_SCREENS) != Config::GBAScreens::Hidden &&
        s_defaults[port + 1].GetHeight() > 0;
  s_selected = 0;
  s_touch_down = false;
  s_error.clear();
  s_editing = true;
  s_last_input = std::chrono::steady_clock::now();
}

void CancelEdit()
{
  std::lock_guard lock(s_mutex);
  if (s_editing)
    s_layout = s_before_edit;
  s_editing = false;
}

bool UpdateEditor(std::uint64_t down, std::uint64_t held)
{
  std::lock_guard lock(s_mutex);
  if (down & HidNpadButton_Plus)
  {
    if (Save())
      return true;
    s_error = "Could not save the game layout. Press + to retry or B to cancel.";
  }
  if (down & (HidNpadButton_L | HidNpadButton_R))
  {
    do
      s_selected = (s_selected + ((down & HidNpadButton_L) ? SCREEN_COUNT - 1 : 1)) % SCREEN_COUNT;
    while (!s_available[s_selected]);
    s_touch_down = false;
  }
  if (down & HidNpadButton_Y)
  {
    s_layout[s_selected] = {};
    s_rectangles[s_selected] = s_defaults[s_selected];
  }
  Rectangle rectangle = s_rectangles[s_selected];
  if (rectangle.GetHeight() <= 0)
    return false;
  if (down & HidNpadButton_X)
  {
    const float remaining = std::max(0.0f, s_width - rectangle.GetWidth());
    const float next = rectangle.left < remaining * 0.25f ? remaining * 0.5f :
                       rectangle.left < remaining * 0.75f ? remaining :
                                                            0.0f;
    rectangle.right = next + rectangle.GetWidth();
    rectangle.left = next;
    SetRectangle(rectangle);
  }
  const auto now = std::chrono::steady_clock::now();
  const float dt =
      std::clamp(std::chrono::duration<float>(now - s_last_input).count(), 0.0f, 0.05f);
  s_last_input = now;
  const float dx = ((held & HidNpadButton_Right) != 0) - ((held & HidNpadButton_Left) != 0);
  const float dy = ((held & HidNpadButton_Down) != 0) - ((held & HidNpadButton_Up) != 0);
  const float resize = ((held & HidNpadButton_ZR) != 0) - ((held & HidNpadButton_ZL) != 0);
  if (dx || dy || resize)
  {
    rectangle = s_rectangles[s_selected];
    const float width = rectangle.GetWidth();
    const float height = rectangle.GetHeight();
    const float change = resize * s_width * dt * 0.3f;
    rectangle.left += dx * s_width * dt * 0.3f - change * 0.5f;
    rectangle.top += dy * s_height * dt * 0.3f - change * height / width * 0.5f;
    rectangle.right = rectangle.left + width + change;
    rectangle.bottom = rectangle.top + height + change * height / width;
    SetRectangle(rectangle);
  }
  const auto touch = ciface::Switch::GetTouchscreenState();
  const float x = touch.x * s_width / 1280;
  const float y = touch.y * s_height / 720;
  if (touch.pressed && !s_touch_down)
  {
    for (std::size_t index = SCREEN_COUNT; index-- > 0;)
    {
      const Rectangle& candidate = s_rectangles[index];
      if (!s_available[index] || x < candidate.left || x > candidate.right || y < candidate.top ||
          y > candidate.bottom)
        continue;
      s_selected = index;
      s_touch_x = x;
      s_touch_y = y;
      s_touch_start = candidate;
      const float handle = 32 * s_height / 720;
      s_resizing = x > candidate.right - handle && y > candidate.bottom - handle;
      s_touch_down = true;
      break;
    }
  }
  else if (touch.pressed && s_touch_down)
  {
    rectangle = s_touch_start;
    if (s_resizing)
    {
      const float aspect = rectangle.GetWidth() / rectangle.GetHeight();
      const float change = ((x - s_touch_x) + (y - s_touch_y) * aspect) * 0.5f;
      rectangle.right += change;
      rectangle.bottom += change / aspect;
    }
    else
    {
      rectangle.left += x - s_touch_x;
      rectangle.right += x - s_touch_x;
      rectangle.top += y - s_touch_y;
      rectangle.bottom += y - s_touch_y;
    }
    SetRectangle(rectangle, true);
  }
  if (!touch.pressed)
    s_touch_down = false;
  return false;
}

void DrawEditor()
{
  std::lock_guard lock(s_mutex);
  ImDrawList* draw = ImGui::GetForegroundDrawList();
  const float scale = s_height / 720;
  for (std::size_t index = 0; index < SCREEN_COUNT; ++index)
  {
    if (!s_available[index])
      continue;
    const Rectangle& rect = s_rectangles[index];
    const ImU32 color =
        index == s_selected ? IM_COL32(100, 225, 255, 255) : IM_COL32(255, 255, 255, 180);
    draw->AddRect({rect.left, rect.top}, {rect.right, rect.bottom}, color, 0, 0, 3 * scale);
    const std::string label = index == 0 ? "Game screen" : "GBA " + std::to_string(index);
    draw->AddText(nullptr, 22 * scale, {rect.left + 8 * scale, rect.top + 8 * scale}, color,
                  label.c_str());
    if (index == s_selected)
      draw->AddRectFilled({rect.right - 28 * scale, rect.bottom - 28 * scale},
                          {rect.right, rect.bottom}, color);
  }
  draw->AddRectFilled({0, s_height - 76 * scale}, {s_width, s_height}, IM_COL32(7, 28, 44, 240));
  draw->AddText(nullptr, 22 * scale, {16 * scale, s_height - 66 * scale}, IM_COL32_WHITE,
                "L / R  Screen    D-Pad  Move    ZL / ZR  Size    X  Align    Y  Reset    B  "
                "Cancel    +  Save game");
  draw->AddText(nullptr, 20 * scale, {16 * scale, s_height - 34 * scale}, IM_COL32_WHITE,
                s_error.empty() ? "Drag a screen to move it, or its bottom-right handle to resize. "
                                  "Aspect ratios are preserved." :
                                  s_error.c_str());
}
}  // namespace DolphinSwitch::ScreenLayout
