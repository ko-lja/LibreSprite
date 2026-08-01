// Aseprite    | Copyright (C) 2001-2016  David Capello
// LibreSprite | Copyright (C) 2021       LibreSprite contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/main_window.h"

#include "app/app.h"
#include "app/app_menus.h"
#include "app/commands/commands.h"
#include "app/ini_file.h"
#include "app/modules/editors.h"
#include "app/pref/preferences.h"
#include "app/ui/color_bar.h"
#include "app/ui/touch_bar.h"
#include "app/ui/context_bar.h"
#include "app/ui/devconsole_view.h"
#include "app/ui/document_view.h"
#include "app/ui/editor/editor.h"
#include "app/ui/editor/editor_view.h"
#include "app/ui/folder_view.h"
#include "app/ui/home_view.h"
#include "app/ui/main_menu_bar.h"
#include "app/ui/notifications.h"
#include "app/ui/preview_editor.h"
#include "app/ui/skin/skin_property.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/status_bar.h"
#include "app/ui/timeline.h"
#include "app/ui/toolbar.h"
#include "app/ui/workspace.h"
#include "app/ui/workspace_tabs.h"
#include "app/ui_context.h"
#include "base/path.h"
#include "ui/message.h"
#include "ui/splitter.h"
#include "ui/system.h"
#include "ui/view.h"

#include <vector>

namespace app {

using namespace ui;

MainWindow::MainWindow()
  : m_mode(NormalMode)
  , m_homeView(nullptr)
  , m_devConsoleView(nullptr)
  , m_attachedFolderView(nullptr)
{
  // Load all menus by first time.
  AppMenus::instance()->reload();

  bool altTouchBar = Preferences::instance().touchBar.alternatePosition();
  auto touchbarParent = altTouchBar ? touchBarAltPlaceholder() : touchBarPlaceholder();

  m_menuBar = new MainMenuBar();
  m_notifications = new Notifications();
  m_contextBar = new ContextBar();
  m_statusBar = new StatusBar();
  m_colorBar = new ColorBar(colorBarPlaceholder()->align());
  m_toolBar = new ToolBar();
  m_tabsBar = new WorkspaceTabs(this);
  m_workspace = new Workspace();
  m_previewEditor = new PreviewEditorWindow();
  m_timeline = new Timeline();
  m_touchBar = TouchBar::create(touchbarParent->align());

  m_workspace->setTabsBar(m_tabsBar);
  m_workspace->ActiveViewChanged.connect(&MainWindow::onActiveViewChange, this);

  // configure all widgets to expansives
  m_menuBar->setExpansive(true);
  m_contextBar->setExpansive(true);
  m_contextBar->setVisible(false);
  m_statusBar->setExpansive(true);
  m_colorBar->setExpansive(true);
  m_touchBar->setExpansive(true);
  m_toolBar->setExpansive(true);
  m_tabsBar->setExpansive(true);
  m_timeline->setExpansive(true);
  m_workspace->setExpansive(true);
  m_notifications->setVisible(false);

  // Setup the menus
  m_menuBar->setMenu(AppMenus::instance()->getRootMenu());

  // Add the widgets in the boxes
  menuBarPlaceholder()->addChild(m_menuBar);
  menuBarPlaceholder()->addChild(m_notifications);
  contextBarPlaceholder()->addChild(m_contextBar);
  colorBarPlaceholder()->addChild(m_colorBar);

  touchbarParent->addChild(m_touchBar.get());

  bool leftToolbar = Preferences::instance().general.leftToolBar();
  if (!has_config_value("general", "left_tool_bar")) {
    // TODO: Decide default based on DPI
#if defined(ANDROID)
    leftToolbar = true;
#endif
  }
  auto toolbarParent = leftToolbar ? toolBarAltPlaceholder() : this->toolBarPlaceholder();
  toolbarParent->addChild(m_toolBar);

  statusBarPlaceholder()->addChild(m_statusBar);
  tabsPlaceholder()->addChild(m_tabsBar);
  workspacePlaceholder()->addChild(m_workspace);
  timelinePlaceholder()->addChild(m_timeline);

  // Default splitter positions
  folderViewSplitter()->setPosition(180*guiscale());
  colorBarSplitter()->setPosition(m_colorBar->sizeHint().w);
  timelineSplitter()->setPosition(75);

  bool verticalTimeline = Preferences::instance().general.verticalTimeline();
  if (!has_config_value("general", "vertical_timeline")) {
    // TODO: Decide default based on DPI
#if defined(ANDROID)
    verticalTimeline = true;
#endif
  }
  timelineSplitter()->setAlign(verticalTimeline ? HORIZONTAL : VERTICAL);

  bool touchBarVisible = Preferences::instance().touchBar.visible();
  if (!has_config_value("touch_bar", "visible")) {
    // TODO: Decide default based on DPI
#if defined(ANDROID)
    touchBarVisible = true;
#endif
    Preferences::instance().touchBar.visible(touchBarVisible);
  }

  // Prepare the window
  remapWindow();

  AppMenus::instance()->rebuildRecentList();
}

void MainWindow::alternateTimeline() {
  auto old = Preferences::instance().general.verticalTimeline();
  Preferences::instance().general.verticalTimeline(!old);
  timelineSplitter()->setAlign(Preferences::instance().general.verticalTimeline() ? HORIZONTAL : VERTICAL);
  configureWorkspaceLayout();
}

void MainWindow::alternateToolbar() {
  auto left = !Preferences::instance().general.leftToolBar();
  Preferences::instance().general.leftToolBar(left);
  auto parent = left ? toolBarAltPlaceholder() : toolBarPlaceholder();
  m_toolBar->parent()->removeChild(m_toolBar);
  parent->addChild(m_toolBar);
  remapWindow();
}

void MainWindow::alternateTouchbar() {
  auto left = !Preferences::instance().touchBar.alternatePosition();
  Preferences::instance().touchBar.alternatePosition(left);
  bool altTouchBar = Preferences::instance().touchBar.alternatePosition();
  auto touchbarParent = altTouchBar ? touchBarAltPlaceholder() : touchBarPlaceholder();
  m_touchBar->parent()->removeChild(m_touchBar.get());
  touchbarParent->addChild(m_touchBar.get());
  remapWindow();
}

void MainWindow::toggleTouchbar() {
  auto visible = !Preferences::instance().touchBar.visible();
  Preferences::instance().touchBar.visible(visible);
  configureWorkspaceLayout();
}

MainWindow::~MainWindow()
{
  std::vector<FolderView*> folderViews;
  for (auto* view : *m_workspace) {
    if (auto* folderView = dynamic_cast<FolderView*>(view))
      folderViews.push_back(folderView);
  }
  detachFolderExplorer();
  detachFolderWorkspace();
  for (auto* folderView : folderViews) {
    m_workspace->removeView(folderView);
    delete folderView;
  }

  if (m_devConsoleView) {
    if (m_devConsoleView->parent())
      m_workspace->removeView(m_devConsoleView);
    delete m_devConsoleView;
  }
  if (m_homeView) {
    if (m_homeView->parent())
      m_workspace->removeView(m_homeView);
    delete m_homeView;
  }
  delete m_contextBar;
  delete m_previewEditor;

  // Destroy the workspace first so ~Editor can dettach slots from
  // ColorBar. TODO this is a terrible hack for slot/signal stuff,
  // connections should be handle in a better/safer way.
  delete m_workspace;

  // Remove the root-menu from the menu-bar (because the rootmenu
  // module should destroy it).
  m_menuBar->setMenu(NULL);
}

DocumentView* MainWindow::getDocView()
{
  if (auto* folderView = dynamic_cast<FolderView*>(m_workspace->activeView()))
    return folderView->activeDocumentView();
  return dynamic_cast<DocumentView*>(m_workspace->activeView());
}

FolderView* MainWindow::activeFolderView() const
{
  return dynamic_cast<FolderView*>(m_workspace->activeView());
}

void MainWindow::openFolder(const std::string& path)
{
  const auto normalized = base::normalize_path(path);
  for (auto* view : *m_workspace) {
    auto* folderView = dynamic_cast<FolderView*>(view);
    if (folderView &&
        base::compare_filenames(folderView->path(), normalized) == 0) {
      m_workspace->setActiveView(folderView);
      m_tabsBar->selectTab(folderView);
      return;
    }
  }

  auto* folderView = new FolderView(normalized);
  m_workspace->addView(folderView);
  m_workspace->setActiveView(folderView);
  m_tabsBar->selectTab(folderView);
}

void MainWindow::folderViewSelected(FolderView* folderView)
{
  if (activeFolderView() != folderView)
    return;
  folderView->refreshExplorer();
  auto* explorer = folderView->explorerWidget();
  if (explorer->parent() != folderViewPlaceholder()) {
    detachFolderExplorer();
    folderViewPlaceholder()->addChild(explorer);
  }
  attachFolderWorkspace(folderView);
  folderViewPlaceholder()->setVisible(m_mode == NormalMode);
  configureWorkspaceLayout();
}

void MainWindow::folderViewClosed(FolderView* folderView)
{
  if (folderView->explorerWidget()->parent() == folderViewPlaceholder())
    folderViewPlaceholder()->removeChild(folderView->explorerWidget());
  if (m_attachedFolderView == folderView)
    detachFolderWorkspace();
}

void MainWindow::detachFolderExplorer()
{
  if (!folderViewPlaceholder()->children().empty())
    folderViewPlaceholder()->removeChild(folderViewPlaceholder()->children().front());
  folderViewPlaceholder()->setVisible(false);
}

void MainWindow::attachFolderWorkspace(FolderView* folderView)
{
  if (m_attachedFolderView != folderView) {
    detachFolderWorkspace();
    folderView->attachWorkspaceWidget(workspacePlaceholder());
    m_attachedFolderView = folderView;
  }
  m_workspace->setVisible(false);
  folderView->workspaceWidget()->setVisible(true);
}

void MainWindow::detachFolderWorkspace()
{
  if (m_attachedFolderView) {
    m_attachedFolderView->restoreWorkspaceWidget();
    m_attachedFolderView = nullptr;
  }
  m_workspace->setVisible(true);
}

HomeView* MainWindow::getHomeView()
{
  if (!m_homeView)
    m_homeView = new HomeView;
  return m_homeView;
}

void MainWindow::reloadMenus()
{
  m_menuBar->reload();

  layout();
  invalidate();
}

void MainWindow::showNotification(INotificationDelegate* del)
{
  m_notifications->addLink(del);
  m_notifications->setVisible(true);
  m_notifications->parent()->layout();
}

void MainWindow::showHomeOnOpen()
{
  if (!getHomeView()->parent()) {
    TabView* selectedTab = m_tabsBar->getSelectedTab();

    // Show "Home" tab in the first position, and select it only if
    // there is no other view selected.
    m_workspace->addView(m_homeView, 0);
    if (selectedTab)
      m_tabsBar->selectTab(selectedTab);
    else
      m_tabsBar->selectTab(m_homeView);
  }
}

void MainWindow::showHome()
{
  if (!getHomeView()->parent()) {
    m_workspace->addView(m_homeView, 0);
  }
  m_tabsBar->selectTab(m_homeView);
}

bool MainWindow::isHomeSelected()
{
  return (m_tabsBar->getSelectedTab() == m_homeView && m_homeView);
}

void MainWindow::showDevConsole()
{
  if (!m_devConsoleView)
    m_devConsoleView = new DevConsoleView;

  if (!m_devConsoleView->parent()) {
    m_workspace->addView(m_devConsoleView);
    m_tabsBar->selectTab(m_devConsoleView);
  }
}

void MainWindow::setMode(Mode mode)
{
  // Check if we already are in the given mode.
  if (m_mode == mode)
    return;

  m_mode = mode;
  configureWorkspaceLayout();
}

bool MainWindow::getTimelineVisibility() const
{
  return Preferences::instance().general.visibleTimeline();
}

void MainWindow::setTimelineVisibility(bool visible)
{
  Preferences::instance().general.visibleTimeline(visible);

  configureWorkspaceLayout();
}

void MainWindow::popTimeline()
{
  Preferences& preferences = Preferences::instance();

  if (!preferences.general.autoshowTimeline())
    return;

  if (!getTimelineVisibility())
    setTimelineVisibility(true);
}

void MainWindow::showDataRecovery(crash::DataRecovery* dataRecovery)
{
  getHomeView()->showDataRecovery(dataRecovery);
}

bool MainWindow::onProcessMessage(ui::Message* msg)
{
  if (msg->type() == kOpenMessage)
    showHomeOnOpen();

  return Window::onProcessMessage(msg);
}

void MainWindow::onSaveLayout(SaveLayoutEvent& ev)
{
  Window::onSaveLayout(ev);
}

// When the active view is changed from methods like
// Workspace::splitView(), this function is called, and we have to
// inform to the UIContext that the current view has changed.
void MainWindow::onActiveViewChange()
{
  if (auto* folderView = activeFolderView()) {
    folderViewSelected(folderView);
    UIContext::instance()->setActiveView(folderView->activeDocumentView());
  }
  else {
    detachFolderExplorer();
    detachFolderWorkspace();
    if (DocumentView* docView = getDocView())
      UIContext::instance()->setActiveView(docView);
    else
      UIContext::instance()->setActiveView(nullptr);
  }

  configureWorkspaceLayout();
}

bool MainWindow::isTabModified(Tabs* tabs, TabView* tabView)
{
  if (DocumentView* docView = dynamic_cast<DocumentView*>(tabView)) {
    Document* document = docView->document();
    return document->isModified();
  }
  else if (auto* folderView = dynamic_cast<FolderView*>(tabView)) {
    return folderView->hasModifiedDocuments();
  }
  else {
    return false;
  }
}

bool MainWindow::canCloneTab(Tabs* tabs, TabView* tabView)
{
  ASSERT(tabView)

  WorkspaceView* view = dynamic_cast<WorkspaceView*>(tabView);
  return view->canCloneWorkspaceView();
}

void MainWindow::onSelectTab(Tabs* tabs, TabView* tabView)
{
  if (!tabView)
    return;

  WorkspaceView* view = dynamic_cast<WorkspaceView*>(tabView);
  if (m_workspace->activeView() != view)
    m_workspace->setActiveView(view);
}

void MainWindow::onCloseTab(Tabs* tabs, TabView* tabView)
{
  WorkspaceView* view = dynamic_cast<WorkspaceView*>(tabView);
  ASSERT(view);
  if (view)
    m_workspace->closeView(view, false);
}

void MainWindow::onCloneTab(Tabs* tabs, TabView* tabView, int pos)
{
  EditorView::SetScrollUpdateMethod(EditorView::KeepOrigin);

  WorkspaceView* view = dynamic_cast<WorkspaceView*>(tabView);
  WorkspaceView* clone = view->cloneWorkspaceView();
  ASSERT(clone);

  m_workspace->addViewToPanel(
    static_cast<WorkspaceTabs*>(tabs)->panel(), clone, true, pos);

  clone->onClonedFrom(view);
}

void MainWindow::onContextMenuTab(Tabs* tabs, TabView* tabView)
{
  WorkspaceView* view = dynamic_cast<WorkspaceView*>(tabView);
  ASSERT(view);
  if (view)
    view->onTabPopup(m_workspace);
}

void MainWindow::onMouseOverTab(Tabs* tabs, TabView* tabView)
{
  // Note: tabView can be NULL
  if (DocumentView* docView = dynamic_cast<DocumentView*>(tabView)) {
    Document* document = docView->document();

    std::string name;
    if (Preferences::instance().general.showFullPath())
      name = document->filename();
    else
      name = base::get_file_name(document->filename());

    m_statusBar->setStatusText(250, "%s", name.c_str());
  }
  else if (auto* folderView = dynamic_cast<FolderView*>(tabView)) {
    m_statusBar->setStatusText(250, "%s", folderView->path().c_str());
  }
  else {
    m_statusBar->clearText();
  }
}

DropViewPreviewResult MainWindow::onFloatingTab(Tabs* tabs, TabView* tabView, const gfx::Point& pos)
{
  if (activeFolderView() || dynamic_cast<FolderView*>(tabView)) {
    m_workspace->removeDropViewPreview();
    return DropViewPreviewResult::DROP_IN_TABS;
  }
  return m_workspace->setDropViewPreview(pos,
    dynamic_cast<WorkspaceView*>(tabView),
    static_cast<WorkspaceTabs*>(tabs));
}

void MainWindow::onDockingTab(Tabs* tabs, TabView* tabView)
{
  m_workspace->removeDropViewPreview();
}

DropTabResult MainWindow::onDropTab(Tabs* tabs, TabView* tabView, const gfx::Point& pos, bool clone)
{
  m_workspace->removeDropViewPreview();

  if (activeFolderView() || dynamic_cast<FolderView*>(tabView))
    return DropTabResult::NOT_HANDLED;

  DropViewAtResult result =
    m_workspace->dropViewAt(pos, dynamic_cast<WorkspaceView*>(tabView), clone);

  if (result == DropViewAtResult::MOVED_TO_OTHER_PANEL)
    return DropTabResult::REMOVE;
  else if (result == DropViewAtResult::CLONED_VIEW)
    return DropTabResult::DONT_REMOVE;
  else
    return DropTabResult::NOT_HANDLED;
}

void MainWindow::configureWorkspaceLayout()
{
  bool normal = (m_mode == NormalMode);
  bool isDoc = (getDocView() != nullptr);
  bool isFolder = (activeFolderView() != nullptr);

  m_menuBar->setVisible(normal);
  m_tabsBar->setVisible(normal);
  colorBarPlaceholder()->setVisible(normal && isDoc);
  folderViewPlaceholder()->setVisible(normal && isFolder);
  m_touchBar->setVisible(normal && isDoc && Preferences::instance().touchBar.visible());
  m_toolBar->setVisible(normal && isDoc);
  m_statusBar->setVisible(normal);
  m_contextBar->setVisible(
    isDoc &&
    (m_mode == NormalMode ||
     m_mode == ContextBarAndTimelineMode));
  timelinePlaceholder()->setVisible(
    isDoc &&
    (m_mode == NormalMode ||
     m_mode == ContextBarAndTimelineMode) &&
    Preferences::instance().general.visibleTimeline());

  if (m_contextBar->isVisible()) {
    m_contextBar->updateForActiveTool();
  }

  layout();
  invalidate();
}

} // namespace app
