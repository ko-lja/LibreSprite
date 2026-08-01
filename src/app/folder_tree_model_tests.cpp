// LibreSprite
// Copyright (C) 2026 LibreSprite contributors

#include <gtest/gtest.h>

#include "app/ui/folder_tree_model.h"
#include "base/path.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace app {
namespace {

class FolderTreeModelTest : public testing::Test {
protected:
  void SetUp() override {
    const auto suffix = std::to_string(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
    m_root = std::filesystem::temp_directory_path() /
             std::filesystem::u8path("libresprite-folder-tree-" + suffix);
    std::filesystem::create_directories(m_root);
  }

  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(m_root, error);
  }

  void touch(const std::filesystem::path& path) {
    std::ofstream stream(path.string());
    stream << "test";
  }

  std::filesystem::path m_root;
};

TEST_F(FolderTreeModelTest, FiltersAndSortsFoldersBeforeFiles)
{
  std::filesystem::create_directory(m_root / "z-folder");
  std::filesystem::create_directory(m_root / "a-folder");
  touch(m_root / "B.PNG");
  touch(m_root / "a.aseprite");
  touch(m_root / "hidden.txt");

  FolderTreeModel model(m_root.u8string(), "ase,aseprite,png");
  ASSERT_EQ(5u, model.entries().size());
  EXPECT_EQ("a-folder", base::get_file_name(model.entries()[1].path));
  EXPECT_EQ("z-folder", base::get_file_name(model.entries()[2].path));
  EXPECT_EQ("a.aseprite", base::get_file_name(model.entries()[3].path));
  EXPECT_EQ("B.PNG", base::get_file_name(model.entries()[4].path));
}

TEST_F(FolderTreeModelTest, ExpansionIsLazyAndPreservedOnRefresh)
{
  const auto folder = m_root / "sprites";
  std::filesystem::create_directory(folder);
  touch(folder / "hero.ase");

  FolderTreeModel model(m_root.u8string(), "ase");
  ASSERT_EQ(2u, model.entries().size());
  model.setExpanded(folder.u8string(), true);
  ASSERT_EQ(3u, model.entries().size());
  model.refresh();
  EXPECT_EQ(3u, model.entries().size());
}

TEST_F(FolderTreeModelTest, ValidatesNamesAndPathBoundaries)
{
  EXPECT_TRUE(FolderTreeModel::isValidName("hero sprites"));
  EXPECT_FALSE(FolderTreeModel::isValidName(""));
  EXPECT_FALSE(FolderTreeModel::isValidName("."));
#ifdef _WIN32
  EXPECT_FALSE(FolderTreeModel::isValidName("bad:name"));
#endif
  EXPECT_TRUE(FolderTreeModel::isInside(
    (m_root / "sprites" / "hero.ase").u8string(), m_root.u8string()));
  EXPECT_FALSE(FolderTreeModel::isInside(
    (m_root.parent_path() / (m_root.filename().u8string() + "-other") /
      "hero.ase").u8string(), m_root.u8string()));
}

TEST_F(FolderTreeModelTest, RecursiveDeleteDoesNotFollowDirectorySymlinks)
{
  const auto outside = m_root.parent_path() /
    std::filesystem::u8path(m_root.filename().u8string() + "-outside");
  const auto folder = m_root / "delete-me";
  std::filesystem::create_directories(outside);
  std::filesystem::create_directories(folder);
  touch(outside / "keep.ase");

  std::error_code error;
  std::filesystem::create_directory_symlink(outside, folder / "link", error);
  FolderTreeModel::removeRecursively(folder.u8string());

  EXPECT_FALSE(std::filesystem::exists(folder));
  EXPECT_TRUE(std::filesystem::exists(outside / "keep.ase"));
  std::filesystem::remove_all(outside, error);
}

} // anonymous namespace
} // namespace app

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
