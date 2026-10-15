// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "DiscIO/RiivolutionPatcher.h"

class RiivolutionPatcherTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_root = File::CreateTempDir();
    ASSERT_FALSE(m_root.empty());
    ASSERT_TRUE(File::CreateDirs(m_root + "/riivolution/Data"));
    ASSERT_TRUE(File::WriteStringToFile(m_root + "/riivolution/Data/Code.bin", "CODE"));
  }

  void TearDown() override
  {
    if (!m_root.empty())
      File::DeleteDirRecursively(m_root);
  }

  std::string m_root;
};

TEST_F(RiivolutionPatcherTest, ReusesFolderMetadataForFilePatches)
{
  DiscIO::Riivolution::FileDataLoaderHostFS loader(m_root, m_root + "/riivolution/test.xml", "");
  for (int repeat = 0; repeat < 2; ++repeat)
  {
    const auto files = loader.GetFolderContents("Data");
    ASSERT_EQ(files.size(), 1u);
    EXPECT_EQ(files[0].m_filename, "Code.bin");
    EXPECT_FALSE(files[0].m_is_directory);
    EXPECT_EQ(loader.GetExternalFileSize("Data/Code.bin"), 4u);
    EXPECT_EQ(loader.GetExternalFileSize("data/code.BIN"), 4u);
    const auto source = loader.MakeContentSource("Data/Code.bin", 1, 2, 3);
    EXPECT_EQ(
        StringToPath(std::get<DiscIO::ContentFile>(source.m_source).m_filename).lexically_normal(),
        StringToPath(m_root + "/riivolution/Data/Code.bin"));
    EXPECT_EQ(source.m_offset, 3u);
    EXPECT_EQ(source.m_size, 2u);
  }
  EXPECT_FALSE(loader.GetExternalFileSize("Data/Missing.bin"));
  EXPECT_EQ(loader.GetFileContents("Data/Code.bin"), std::vector<u8>({'C', 'O', 'D', 'E'}));
  ASSERT_TRUE(File::WriteStringToFile(m_root + "/riivolution/Data/Code.bin", "NEW"));
  EXPECT_EQ(loader.GetFileContents("Data/Code.bin"), std::vector<u8>({'N', 'E', 'W'}));
}

TEST_F(RiivolutionPatcherTest, KeepsRootsAndTraversalValidation)
{
  DiscIO::Riivolution::FileDataLoaderHostFS relative(m_root, m_root + "/riivolution/test.xml",
                                                     "Data");
  DiscIO::Riivolution::FileDataLoaderHostFS absolute(m_root, m_root + "/riivolution/test.xml",
                                                     "/riivolution/Data");
  for (auto* loader : {&relative, &absolute})
  {
    EXPECT_EQ(loader->GetExternalFileSize("Code.bin"), 4u);
    EXPECT_EQ(loader->GetExternalFileSize("/riivolution/Data/Code.bin"), 4u);
    EXPECT_EQ(loader->GetExternalFileSize("./Code.bin"), 4u);
    EXPECT_FALSE(loader->GetExternalFileSize("../Data/Code.bin"));
    EXPECT_FALSE(loader->GetExternalFileSize("/../riivolution/Data/Code.bin"));
  }
}
