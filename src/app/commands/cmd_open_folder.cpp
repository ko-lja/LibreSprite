// LibreSprite
// Copyright (C) 2026 LibreSprite contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/commands/command.h"
#include "app/file_selector.h"
#include "app/ui/main_window.h"
#include "base/fs.h"

namespace app {

class OpenFolderCommand : public Command {
public:
  OpenFolderCommand()
    : Command("OpenFolder", "Open Folder", CmdUIOnlyFlag) {
  }

  Command* clone() const override { return new OpenFolderCommand(*this); }

protected:
  void onExecute(Context*) override {
    auto* mainWindow = App::instance()->mainWindow();
    if (!mainWindow)
      return;

    const auto folder = show_folder_selector(
      "Open Folder", base::get_user_docs_folder());
    if (!folder.empty())
      mainWindow->openFolder(folder);
  }
};

Command* CommandFactory::createOpenFolderCommand()
{
  return new OpenFolderCommand;
}

} // namespace app
