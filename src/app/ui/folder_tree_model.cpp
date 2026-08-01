// LibreSprite
// Copyright (C) 2026 LibreSprite contributors

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/folder_tree_model.h"

#include "base/fs.h"
#include "base/path.h"
#include "base/string.h"

#include <algorithm>
#include <filesystem>

namespace app {

FolderTreeModel::FolderTreeModel(
  const std::string& root,
  const std::string& extensions)
  : m_root(base::normalize_path(root))
  , m_extensions(extensions)
{
  m_expanded.insert(m_root);
  refresh();
}

void FolderTreeModel::refresh()
{
  m_entries.clear();
  appendFolder(m_root, 0);
}

void FolderTreeModel::setExpanded(const std::string& path, bool expanded)
{
  const auto normalized = base::normalize_path(path);
  if (expanded)
    m_expanded.insert(normalized);
  else
    m_expanded.erase(normalized);
  refresh();
}

void FolderTreeModel::rewriteExpandedPaths(
  const std::string& oldPath,
  const std::string& newPath)
{
  std::set<std::string> rewritten;
  for (const auto& expanded : m_expanded) {
    if (isInside(expanded, oldPath))
      rewritten.insert(newPath + expanded.substr(oldPath.size()));
    else
      rewritten.insert(expanded);
  }
  m_expanded.swap(rewritten);
  refresh();
}

bool FolderTreeModel::isExpanded(const std::string& path) const
{
  const auto normalized = base::normalize_path(path);
  for (const auto& expanded : m_expanded) {
    if (base::compare_filenames(expanded, normalized) == 0)
      return true;
  }
  return false;
}

bool FolderTreeModel::isSupportedFile(const std::string& path) const
{
  return base::has_file_extension(path, m_extensions);
}

bool FolderTreeModel::isValidName(const std::string& name)
{
  return !name.empty() && name != "." && name != ".." &&
         base::verify_filename(name) == std::string::npos;
}

bool FolderTreeModel::isInside(
  const std::string& path,
  const std::string& folder)
{
  const auto normalizedPath = base::normalize_path(path);
  auto normalizedFolder = base::normalize_path(folder);
  if (base::compare_filenames(normalizedPath, normalizedFolder) == 0)
    return true;

  normalizedFolder = base::join_path(normalizedFolder, std::string());
  if (normalizedPath.size() < normalizedFolder.size())
    return false;
  return base::compare_filenames(
           normalizedPath.substr(0, normalizedFolder.size()),
           normalizedFolder) == 0;
}

bool FolderTreeModel::isSymlink(const std::string& path)
{
  std::error_code error;
  return std::filesystem::is_symlink(
    std::filesystem::symlink_status(std::filesystem::u8path(path), error));
}

void FolderTreeModel::removeRecursively(const std::string& path)
{
  const auto fsPath = std::filesystem::u8path(path);
  std::error_code error;
  const auto status = std::filesystem::symlink_status(fsPath, error);
  if (error)
    throw std::filesystem::filesystem_error("Cannot inspect path", fsPath, error);

  if (std::filesystem::is_symlink(status) ||
      !std::filesystem::is_directory(status)) {
    if (!std::filesystem::remove(fsPath, error) || error)
      throw std::filesystem::filesystem_error("Cannot delete path", fsPath, error);
  }
  else {
    std::filesystem::remove_all(fsPath, error);
    if (error)
      throw std::filesystem::filesystem_error("Cannot delete folder", fsPath, error);
  }
}

void FolderTreeModel::appendFolder(const std::string& path, int depth)
{
  const bool expanded = isExpanded(path);
  m_entries.push_back({ path, depth, true, expanded });
  if (!expanded || isSymlink(path))
    return;

  auto children = base::list_files(path);
  std::sort(children.begin(), children.end(), [&path](const auto& a, const auto& b) {
    const auto pathA = base::join_path(path, a);
    const auto pathB = base::join_path(path, b);
    const bool folderA = base::is_directory(pathA);
    const bool folderB = base::is_directory(pathB);
    if (folderA != folderB)
      return folderA;
    return base::compare_filenames(base::get_file_name(a),
                                   base::get_file_name(b)) < 0;
  });

  for (const auto& childName : children) {
    const auto child = base::get_file_path(childName).empty() ?
      base::join_path(path, childName): childName;
    if (base::is_directory(child))
      appendFolder(child, depth+1);
    else if (base::is_file(child) && isSupportedFile(child))
      m_entries.push_back({ child, depth+1, false, false });
  }
}

} // namespace app
