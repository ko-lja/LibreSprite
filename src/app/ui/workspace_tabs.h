// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#pragma once

#include "app/ui/tabs.h"

namespace app {
  class Workspace;
  class WorkspacePanel;

  class WorkspaceTabs : public Tabs {
  public:
    ui::WidgetType Type();

    WorkspaceTabs(TabsDelegate* tabsDelegate);
    ~WorkspaceTabs();

    WorkspacePanel* panel() const { return m_panel; }
    void setPanel(WorkspacePanel* panel);

    Workspace* workspace() const { return m_workspace; }
    void setWorkspace(Workspace* workspace);

    bool canDockWith(const Tabs* tabs) const override;

  private:
    WorkspacePanel* m_panel;
    Workspace* m_workspace;
  };

} // namespace app
