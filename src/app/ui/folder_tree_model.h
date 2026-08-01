// LibreSprite
// Copyright (C) 2026 LibreSprite contributors

#pragma once

#include <set>
#include <string>
#include <vector>

namespace app {

class FolderTreeModel {
public:
  struct Entry {
    std::string path;
    int depth;
    bool folder;
    bool expanded;
  };

  FolderTreeModel(const std::string& root, const std::string& extensions);

  const std::string& root() const { return m_root; }
  const std::vector<Entry>& entries() const { return m_entries; }

  void refresh();
  void setExpanded(const std::string& path, bool expanded);
  void rewriteExpandedPaths(
    const std::string& oldPath, const std::string& newPath);
  bool isExpanded(const std::string& path) const;
  bool isSupportedFile(const std::string& path) const;

  static bool isValidName(const std::string& name);
  static bool isInside(const std::string& path, const std::string& folder);
  static bool isSymlink(const std::string& path);
  static void removeRecursively(const std::string& path);

private:
  void appendFolder(const std::string& path, int depth);

  std::string m_root;
  std::string m_extensions;
  std::set<std::string> m_expanded;
  std::vector<Entry> m_entries;
};

} // namespace app
