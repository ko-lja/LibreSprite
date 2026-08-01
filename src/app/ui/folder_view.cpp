// LibreSprite
// Copyright (C) 2026 LibreSprite contributors

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/folder_view.h"

#include "app/app.h"
#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/commands/params.h"
#include "app/console.h"
#include "app/document.h"
#include "app/file/file.h"
#include "app/recent_files.h"
#include "app/ui/document_view.h"
#include "app/ui/folder_tree_model.h"
#include "app/ui/main_window.h"
#include "app/ui/status_bar.h"
#include "app/ui/workspace.h"
#include "app/ui/workspace_tabs.h"
#include "app/ui_context.h"
#include "base/bind.h"
#include "base/fs.h"
#include "base/path.h"
#include "ui/alert.h"
#include "ui/listbox.h"
#include "ui/listitem.h"
#include "ui/menu.h"
#include "ui/message.h"
#include "ui/system.h"
#include "ui/view.h"

#include "path_name_window.xml.h"

#include <set>
#include <vector>

namespace app {

using namespace ui;

namespace {

class FolderItem : public ListItem {
public:
  FolderItem(const FolderTreeModel::Entry& entry)
    : ListItem(makeText(entry))
    , path(entry.path)
    , folder(entry.folder)
    , expanded(entry.expanded) {
    setValue(path);
  }

  std::string path;
  bool folder;
  bool expanded;

private:
  static std::string makeText(const FolderTreeModel::Entry& entry) {
    std::string text(entry.depth*2, ' ');
    if (entry.folder)
      text += (entry.expanded ? "[-] ": "[+] ");
    else
      text += "    ";
    text += base::get_file_name(entry.path);
    return text;
  }
};

} // anonymous namespace

class FolderExplorer;

class FolderListBox : public ListBox {
public:
  explicit FolderListBox(FolderExplorer* owner)
    : m_owner(owner) {
  }

protected:
  bool onProcessMessage(Message* msg) override;

private:
  FolderExplorer* m_owner;
};

class FolderExplorer : public VBox {
public:
  explicit FolderExplorer(FolderView* owner)
    : m_owner(owner)
    , m_model(owner->path(), get_readable_extensions())
    , m_view(new View)
    , m_list(new FolderListBox(this))
  {
    noBorderNoChildSpacing();
    setExpansive(true);
    m_view->setExpansive(true);
    addChild(m_view);
    m_view->attachToView(m_list);
    m_list->DoubleClickItem.connect(base::Bind(&FolderExplorer::openSelected, this));
    rebuild();
  }

  void refresh() {
    const auto selected = selectedItem() ? selectedItem()->path: std::string();
    m_model.refresh();
    rebuild(selected);
  }

  FolderItem* selectedItem() const {
    return dynamic_cast<FolderItem*>(m_list->getSelectedChild());
  }

  void selectItemAt(const gfx::Point& position) {
    Widget* picked = m_view->viewport()->pick(position);
    while (picked && picked->parent() != m_list)
      picked = picked->parent();
    if (picked && picked->parent() == m_list)
      m_list->selectChild(picked);
  }

  void clickSelected() {
    auto* item = selectedItem();
    if (item && item->folder) {
      m_model.setExpanded(item->path, !item->expanded);
      rebuild(item->path);
    }
  }

  void openSelected() {
    auto* item = selectedItem();
    if (!item)
      return;
    if (item->folder) {
      clickSelected();
      return;
    }

    auto* context = UIContext::instance();
    if (auto* document = static_cast<Document*>(
          context->documents().getByFileName(item->path))) {
      context->setActiveDocument(document);
      return;
    }

    Params params;
    params.set("filename", item->path.c_str());
    auto* command = CommandsModule::instance()->getCommandByName(CommandId::OpenFile);
    auto* previous = context->documentViewDestination();
    context->setDocumentViewDestination(m_owner);
    context->executeCommand(command, params);
    context->setDocumentViewDestination(previous);
  }

  void expandSelected(bool expand) {
    auto* item = selectedItem();
    if (!item)
      return;
    if (!item->folder) {
      if (!expand) {
        const auto parent = base::get_file_path(item->path);
        rebuild(parent);
      }
      return;
    }
    if (expand != item->expanded) {
      m_model.setExpanded(item->path, expand);
      rebuild(item->path);
    }
  }

  void showContextMenu() {
    Menu menu;
    MenuItem newImage("New Image...");
    MenuItem newFolder("New Folder...");
    MenuSeparator separator;
    MenuItem rename("Rename\tF2");
    MenuItem remove("Delete\tDel");
    MenuItem refresh("Refresh\tF5");
    menu.addChild(&newImage);
    menu.addChild(&newFolder);
    menu.addChild(&separator);
    menu.addChild(&rename);
    menu.addChild(&remove);
    menu.addChild(&refresh);
    newImage.Click.connect(base::Bind(&FolderExplorer::newImage, this));
    newFolder.Click.connect(base::Bind(&FolderExplorer::newFolder, this));
    rename.Click.connect(base::Bind(&FolderExplorer::renameSelected, this));
    remove.Click.connect(base::Bind(&FolderExplorer::deleteSelected, this));
    refresh.Click.connect(base::Bind(&FolderExplorer::refresh, this));
    menu.showPopup(get_mouse_position());
  }

  void newImage() {
    const auto target = targetFolder();
    auto* context = UIContext::instance();
    std::set<doc::Document*> existingDocuments;
    for (auto* document : context->documents())
      existingDocuments.insert(document);
    auto* previous = context->documentViewDestination();
    context->setDocumentViewDestination(m_owner);
    context->executeCommand(
      CommandsModule::instance()->getCommandByName(CommandId::NewFile));
    context->setDocumentViewDestination(previous);

    DocumentView* view = nullptr;
    for (auto* document : context->documents()) {
      if (existingDocuments.find(document) == existingDocuments.end()) {
        view = context->getFirstDocumentView(document);
        break;
      }
    }
    if (view && m_owner->contains(view)) {
      Params params;
      params.set("folder", target.c_str());
      context->executeCommand(
        CommandsModule::instance()->getCommandByName(CommandId::SaveFileAs),
        params);
      refresh();
    }
  }

  void newFolder() {
    std::string name;
    if (!askForName("New Folder", "Folder name:", "New Folder", name))
      return;
    if (!validateName(name))
      return;

    const auto path = base::join_path(targetFolder(), name);
    if (base::is_file(path) || base::is_directory(path)) {
      Alert::show("Error<<A file or folder named \"%s\" already exists.||&OK",
                  name.c_str());
      return;
    }
    try {
      base::make_directory(path);
      m_model.setExpanded(base::get_file_path(path), true);
      rebuild(path);
    }
    catch (const std::exception& error) {
      Console::showException(error);
    }
  }

  void renameSelected() {
    auto* item = selectedItem();
    if (!item || base::compare_filenames(item->path, m_model.root()) == 0)
      return;

    const auto oldPath = item->path;
    const auto oldName = item->folder ?
      base::get_file_name(oldPath): base::get_file_title(oldPath);
    std::string name;
    if (!askForName(item->folder ? "Rename Folder": "Rename Image",
                    item->folder ? "Folder name:": "Image name:",
                    oldName, name))
      return;
    if (!validateName(name))
      return;

    const auto filename = item->folder ? name:
      name + "." + base::get_file_extension(oldPath);
    const auto newPath = base::join_path(base::get_file_path(oldPath), filename);
    if (base::is_file(newPath) || base::is_directory(newPath)) {
      Alert::show("Error<<A file or folder named \"%s\" already exists.||&OK",
                  filename.c_str());
      return;
    }

    try {
      base::move_file(oldPath, newPath);
      if (item->folder)
        m_model.rewriteExpandedPaths(oldPath, newPath);
      rewriteDocumentPaths(oldPath, newPath);
      refresh();
      rebuild(newPath);
      App::instance()->workspace()->updateTabs();
    }
    catch (const std::exception& error) {
      Console::showException(error);
    }
  }

  void deleteSelected() {
    auto* item = selectedItem();
    if (!item || base::compare_filenames(item->path, m_model.root()) == 0)
      return;

    const auto path = item->path;
    if (Alert::show("Warning<<Permanently delete \"%s\"%s?"
                    "||&Delete||&Cancel",
                    base::get_file_name(path).c_str(),
                    item->folder ? " and everything inside it": "") != 1)
      return;

    if (!closeDocumentsUnder(path))
      return;

    try {
      FolderTreeModel::removeRecursively(path);
      refresh();
    }
    catch (const std::exception& error) {
      Console::showException(error);
    }
  }

private:
  void rebuild(const std::string& selectPath = std::string()) {
    auto children = m_list->children();
    for (auto* child : children)
      delete child;

    FolderItem* select = nullptr;
    for (const auto& entry : m_model.entries()) {
      auto* item = new FolderItem(entry);
      m_list->addChild(item);
      if (!selectPath.empty() &&
          base::compare_filenames(entry.path, selectPath) == 0)
        select = item;
    }
    if (select)
      m_list->selectChild(select);
    m_list->layout();
    m_view->updateView();
    if (select)
      m_list->makeChildVisible(select);
    m_list->invalidate();
  }

  std::string targetFolder() const {
    auto* item = selectedItem();
    if (!item)
      return m_model.root();
    return item->folder ? item->path: base::get_file_path(item->path);
  }

  bool askForName(const char* title, const char* label,
                  const std::string& value, std::string& result) {
    gen::PathNameWindow window;
    window.setText(title);
    window.label()->setText(label);
    window.name()->setText(value);
    window.name()->selectText(0, -1);
    window.openWindowInForeground();
    if (window.closer() != window.ok())
      return false;
    result = window.name()->text();
    return true;
  }

  bool validateName(const std::string& name) {
    if (FolderTreeModel::isValidName(name))
      return true;
    Alert::show("Error<<\"%s\" is not a valid name.||&OK", name.c_str());
    return false;
  }

  void rewriteDocumentPaths(
    const std::string& oldPath,
    const std::string& newPath) {
    auto* context = UIContext::instance();
    for (auto* baseDocument : context->documents()) {
      auto* document = static_cast<Document*>(baseDocument);
      if (!FolderTreeModel::isInside(document->filename(), oldPath))
        continue;
      const auto suffix = document->filename().substr(oldPath.size());
      const auto oldFilename = document->filename();
      document->setFilename(newPath + suffix);
      App::instance()->recentFiles()->removeRecentFile(oldFilename.c_str());
      App::instance()->recentFiles()->addRecentFile(document->filename().c_str());
    }
  }

  bool closeDocumentsUnder(const std::string& path) {
    auto* context = UIContext::instance();
    std::set<Document*> documents;
    for (auto* baseDocument : context->documents()) {
      auto* document = static_cast<Document*>(baseDocument);
      if (FolderTreeModel::isInside(document->filename(), path))
        documents.insert(document);
    }

    for (auto* document : documents) {
      while (auto* view = context->getFirstDocumentView(document)) {
        auto* workspace = context->workspaceFor(view);
        if (!workspace || !workspace->closeView(view, false))
          return false;
      }
    }
    return true;
  }

  FolderView* m_owner;
  FolderTreeModel m_model;
  View* m_view;
  FolderListBox* m_list;

  friend class FolderListBox;
};

bool FolderListBox::onProcessMessage(Message* msg)
{
  if (msg->type() == kMouseDownMessage) {
    auto* mouse = static_cast<MouseMessage*>(msg);
    m_owner->selectItemAt(mouse->position());
    if (mouse->right()) {
      m_owner->showContextMenu();
      return true;
    }
  }
  else if (msg->type() == kKeyDownMessage && hasFocus()) {
    const auto key = static_cast<KeyMessage*>(msg)->scancode();
    switch (key) {
      case kKeyEnter:
      case kKeyEnterPad: m_owner->openSelected(); return true;
      case kKeyLeft: m_owner->expandSelected(false); return true;
      case kKeyRight: m_owner->expandSelected(true); return true;
      case kKeyF2: m_owner->renameSelected(); return true;
      case kKeyF5: m_owner->refresh(); return true;
      case kKeyDel:
      case kKeyDelPad: m_owner->deleteSelected(); return true;
      default: break;
    }
  }
  return ListBox::onProcessMessage(msg);
}

FolderView::FolderView(const std::string& path)
  : m_path(base::normalize_path(path))
  , m_explorer(new FolderExplorer(this))
  , m_content(new VBox)
  , m_tabs(new WorkspaceTabs(this))
  , m_workspace(new Workspace)
{
  noBorderNoChildSpacing();
  m_content->noBorderNoChildSpacing();
  m_content->setExpansive(true);
  m_tabs->setDockedStyle();
  m_workspace->setTabsBar(m_tabs);
  m_workspace->ActiveViewChanged.connect(&FolderView::onActiveDocumentChange, this);
  m_afterCommandConnection = UIContext::instance()->AfterCommandExecution.connect(
    &FolderView::onAfterCommandExecution, this);
  m_workspace->setExpansive(true);
  m_content->addChild(m_tabs);
  m_content->addChild(m_workspace);
  addChild(m_content);
}

FolderView::~FolderView()
{
  if (m_explorer->parent())
    m_explorer->parent()->removeChild(m_explorer);
  delete m_explorer;
}

Widget* FolderView::explorerWidget() const
{
  return m_explorer;
}

Widget* FolderView::workspaceWidget() const
{
  return m_content;
}

void FolderView::attachWorkspaceWidget(Widget* parent)
{
  if (m_content->parent() == parent)
    return;
  if (m_content->parent())
    m_content->parent()->removeChild(m_content);
  parent->addChild(m_content);
}

void FolderView::restoreWorkspaceWidget()
{
  attachWorkspaceWidget(this);
}

DocumentView* FolderView::activeDocumentView() const
{
  return dynamic_cast<DocumentView*>(m_workspace->activeView());
}

bool FolderView::contains(DocumentView* documentView) const
{
  for (auto* view : *m_workspace) {
    if (view == documentView)
      return true;
  }
  return false;
}

bool FolderView::hasModifiedDocuments() const
{
  for (auto* view : *m_workspace) {
    auto* documentView = dynamic_cast<DocumentView*>(view);
    if (documentView && documentView->document()->isModified())
      return true;
  }
  return false;
}

void FolderView::addDocumentView(DocumentView* view)
{
  m_workspace->addView(view);
}

void FolderView::refreshExplorer()
{
  m_explorer->refresh();
}

std::string FolderView::getTabText()
{
  return base::get_file_name(m_path);
}

TabIcon FolderView::getTabIcon()
{
  return TabIcon::NONE;
}

bool FolderView::onCloseView(Workspace* workspace, bool quitting)
{
  std::set<Document*> documents;
  for (auto* view : *m_workspace) {
    if (auto* documentView = dynamic_cast<DocumentView*>(view))
      documents.insert(documentView->document());
  }
  for (auto* document : documents) {
    while (true) {
      DocumentView* view = nullptr;
      for (auto* candidate : *m_workspace) {
        auto* documentView = dynamic_cast<DocumentView*>(candidate);
        if (documentView && documentView->document() == document) {
          view = documentView;
          break;
        }
      }
      if (!view)
        break;
      if (!m_workspace->closeView(view, quitting))
        return false;
    }
  }

  App::instance()->mainWindow()->folderViewClosed(this);
  workspace->removeView(this);
  delete this;
  return true;
}

void FolderView::onTabPopup(Workspace*)
{
  Menu menu;
  MenuItem refresh("Refresh Folder");
  menu.addChild(&refresh);
  refresh.Click.connect(base::Bind(&FolderView::refreshExplorer, this));
  menu.showPopup(get_mouse_position());
}

void FolderView::onWorkspaceViewSelected()
{
  refreshExplorer();
  App::instance()->mainWindow()->folderViewSelected(this);
}

InputChainElement* FolderView::onGetInputChainElement()
{
  return m_workspace;
}

bool FolderView::isTabModified(Tabs*, TabView* tabView)
{
  auto* view = dynamic_cast<DocumentView*>(tabView);
  return view && view->document()->isModified();
}

bool FolderView::canCloneTab(Tabs*, TabView* tabView)
{
  auto* view = dynamic_cast<WorkspaceView*>(tabView);
  return view && view->canCloneWorkspaceView();
}

void FolderView::onSelectTab(Tabs*, TabView* tabView)
{
  auto* view = dynamic_cast<WorkspaceView*>(tabView);
  if (view && m_workspace->activeView() != view)
    m_workspace->setActiveView(view);
}

void FolderView::onCloseTab(Tabs*, TabView* tabView)
{
  if (auto* view = dynamic_cast<WorkspaceView*>(tabView))
    m_workspace->closeView(view, false);
}

void FolderView::onCloneTab(Tabs*, TabView* tabView, int pos)
{
  auto* view = dynamic_cast<WorkspaceView*>(tabView);
  if (!view)
    return;
  auto* clone = view->cloneWorkspaceView();
  if (!clone)
    return;
  m_workspace->addView(clone, pos);
  clone->onClonedFrom(view);
}

void FolderView::onContextMenuTab(Tabs*, TabView* tabView)
{
  if (auto* view = dynamic_cast<WorkspaceView*>(tabView))
    view->onTabPopup(m_workspace);
}

void FolderView::onMouseOverTab(Tabs*, TabView* tabView)
{
  if (auto* view = dynamic_cast<DocumentView*>(tabView))
    StatusBar::instance()->setStatusText(250, "%s", view->document()->filename().c_str());
  else
    StatusBar::instance()->clearText();
}

DropViewPreviewResult FolderView::onFloatingTab(
  Tabs* tabs, TabView* tabView, const gfx::Point& pos)
{
  if (App::instance()->mainWindow()->getTabsBar()->bounds().contains(pos)) {
    m_workspace->removeDropViewPreview();
    return DropViewPreviewResult::DROP_IN_TABS;
  }
  return m_workspace->setDropViewPreview(
    pos,
    dynamic_cast<WorkspaceView*>(tabView),
    static_cast<WorkspaceTabs*>(tabs));
}

void FolderView::onDockingTab(Tabs*, TabView*)
{
  m_workspace->removeDropViewPreview();
}

DropTabResult FolderView::onDropTab(
  Tabs*, TabView* tabView, const gfx::Point& pos, bool clone)
{
  m_workspace->removeDropViewPreview();

  if (App::instance()->mainWindow()->getTabsBar()->bounds().contains(pos))
    return DropTabResult::NOT_HANDLED;

  const auto result = m_workspace->dropViewAt(
    pos, dynamic_cast<WorkspaceView*>(tabView), clone);
  if (result == DropViewAtResult::MOVED_TO_OTHER_PANEL)
    return DropTabResult::REMOVE;
  if (result == DropViewAtResult::CLONED_VIEW)
    return DropTabResult::DONT_REMOVE;
  return DropTabResult::NOT_HANDLED;
}

void FolderView::onActiveDocumentChange()
{
  UIContext::instance()->setActiveView(activeDocumentView());
  App::instance()->mainWindow()->folderViewSelected(this);
}

void FolderView::onAfterCommandExecution(CommandExecutionEvent& event)
{
  const auto& id = event.command()->id();
  if (id == CommandId::SaveFile ||
      id == CommandId::SaveFileAs ||
      id == CommandId::SaveFileCopyAs)
    refreshExplorer();
}

} // namespace app
