// LibreSprite
// Copyright (C) 2026 LibreSprite contributors

#pragma once

#include "app/ui/tabs.h"
#include "app/ui/workspace_view.h"
#include "base/signal.h"
#include "ui/box.h"

#include <string>

namespace ui {
class ListBox;
class Widget;
}

namespace app {

class Document;
class CommandExecutionEvent;
class DocumentView;
class FolderExplorer;
class Workspace;
class WorkspaceTabs;

class FolderView : public ui::VBox
                 , public TabView
                 , public WorkspaceView
                 , public TabsDelegate {
public:
  explicit FolderView(const std::string& path);
  ~FolderView() override;

  const std::string& path() const { return m_path; }
  Workspace* workspace() const { return m_workspace; }
  ui::Widget* explorerWidget() const;
  DocumentView* activeDocumentView() const;
  bool contains(DocumentView* view) const;
  bool hasModifiedDocuments() const;
  void addDocumentView(DocumentView* view);
  void refreshExplorer();

  std::string getTabText() override;
  TabIcon getTabIcon() override;

  ui::Widget* getContentWidget() override { return this; }
  bool onCloseView(Workspace* workspace, bool quitting) override;
  void onTabPopup(Workspace* workspace) override;
  void onWorkspaceViewSelected() override;
  InputChainElement* onGetInputChainElement() override;

  bool isTabModified(Tabs* tabs, TabView* tabView) override;
  bool canCloneTab(Tabs* tabs, TabView* tabView) override;
  void onSelectTab(Tabs* tabs, TabView* tabView) override;
  void onCloseTab(Tabs* tabs, TabView* tabView) override;
  void onCloneTab(Tabs* tabs, TabView* tabView, int pos) override;
  void onContextMenuTab(Tabs* tabs, TabView* tabView) override;
  void onMouseOverTab(Tabs* tabs, TabView* tabView) override;
  DropViewPreviewResult onFloatingTab(
    Tabs* tabs, TabView* tabView, const gfx::Point& pos) override;
  void onDockingTab(Tabs* tabs, TabView* tabView) override;
  DropTabResult onDropTab(
    Tabs* tabs, TabView* tabView, const gfx::Point& pos, bool clone) override;

private:
  void onActiveDocumentChange();
  void onAfterCommandExecution(CommandExecutionEvent& event);

  std::string m_path;
  FolderExplorer* m_explorer;
  WorkspaceTabs* m_tabs;
  Workspace* m_workspace;
  base::ScopedConnection m_afterCommandConnection;
};

} // namespace app
