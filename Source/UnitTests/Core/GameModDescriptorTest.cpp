// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "DiscIO/GameModDescriptor.h"
#include "DiscIO/RiivolutionParser.h"

namespace
{
TEST(GameModDescriptor, KeepsDevoptabAndUriPathsRooted)
{
  constexpr std::string_view descriptor_json = R"json(
  {
    "type": "dolphin-game-mod-descriptor",
    "version": 1,
    "base-file": "sdmc:/games/New Super Mario Bros. Wii.rvz",
    "banner": "ums0:/art/banner.png",
    "riivolution": {
      "patches": [
        {
          "xml": "dsmb_example:/riivolution/NewerSMBW.xml",
          "root": "sdmc:"
        },
        {
          "xml": "content://provider/document/patch.xml",
          "root": "ums1:/"
        }
      ]
    }
  })json";

  const auto descriptor = DiscIO::ParseGameModDescriptorString(
      descriptor_json, "sdmc:/switch/dolphin/Load/Riivolution/Presets/test.json");
  ASSERT_TRUE(descriptor.has_value());
  EXPECT_EQ(descriptor->base_file, "sdmc:/games/New Super Mario Bros. Wii.rvz");
  EXPECT_EQ(descriptor->banner, "ums0:/art/banner.png");
  ASSERT_TRUE(descriptor->riivolution.has_value());
  ASSERT_EQ(descriptor->riivolution->patches.size(), 2u);
  EXPECT_EQ(descriptor->riivolution->patches[0].xml,
            "dsmb_example:/riivolution/NewerSMBW.xml");
  EXPECT_EQ(descriptor->riivolution->patches[0].root, "sdmc:");
  EXPECT_EQ(descriptor->riivolution->patches[1].xml,
            "content://provider/document/patch.xml");
  EXPECT_EQ(descriptor->riivolution->patches[1].root, "ums1:/");
}

TEST(GameModDescriptor, PreservesDisabledRiivolutionChoices)
{
  auto disc = DiscIO::Riivolution::ParseString(R"xml(
    <wiidisc version="1">
      <options><section name="Mod">
        <option name="Pack"><choice name="Enabled"><patch id="pack"/></choice></option>
        <option id="music" name="Music" default="1">
          <choice name="Enabled"><patch id="music"/></choice>
        </option>
      </section></options>
      <patch id="pack"/><patch id="music"/>
    </wiidisc>)xml",
                                               "sdmc:/riivolution/test.xml");
  ASSERT_TRUE(disc);
  DiscIO::GameModDescriptor descriptor;
  descriptor.base_file = "sdmc:/games/game.rvz";
  descriptor.riivolution.emplace();
  descriptor.riivolution->patches.push_back(
      {"sdmc:/riivolution/test.xml", "sdmc:", {{"Mod", "", "Pack", 1}, {"Mod", "music", "", 0}}});

  const auto saved = DiscIO::ParseGameModDescriptorString(
      DiscIO::WriteGameModDescriptorString(descriptor, false), "sdmc:/games/mod.json");
  ASSERT_TRUE(saved && saved->riivolution);
  DiscIO::Riivolution::ApplyGameModDescriptorOptions(&*disc, saved->riivolution->patches[0]);
  const auto patches = disc->GeneratePatches("RMCE01");
  ASSERT_EQ(patches.size(), 1u);
  EXPECT_EQ(patches[0].m_id, "pack");
  EXPECT_EQ(disc->m_sections[0].m_options[1].m_selected_choice, 0u);
}
}  // namespace
