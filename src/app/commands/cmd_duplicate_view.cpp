// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/commands/command.h"

#include "app/app.h"
#include "app/ui/workspace.h"
#include "app/ui_context.h"

#include <cstdio>

namespace app {

// using namespace ui;

class DuplicateViewCommand : public Command {
public:
  DuplicateViewCommand();
  Command* clone() const override { return new DuplicateViewCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

DuplicateViewCommand::DuplicateViewCommand()
  : Command("DuplicateView",
            "Duplicate View",
            CmdUIOnlyFlag)
{
}

bool DuplicateViewCommand::onEnabled(Context* context)
{
  Workspace* workspace = UIContext::instance()->activeWorkspace();
  WorkspaceView* view = workspace->activeView();
  return (view != nullptr);
}

void DuplicateViewCommand::onExecute(Context* context)
{
  UIContext::instance()->activeWorkspace()->duplicateActiveView();
}

Command* CommandFactory::createDuplicateViewCommand()
{
  return new DuplicateViewCommand;
}

} // namespace app
