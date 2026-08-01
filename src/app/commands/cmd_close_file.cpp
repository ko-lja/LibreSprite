// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/context_access.h"
#include "app/document_access.h"
#include "app/modules/editors.h"
#include "app/ui/document_view.h"
#include "app/ui/status_bar.h"
#include "app/ui/workspace.h"
#include "app/ui_context.h"
#include "doc/sprite.h"
#include "ui/ui.h"

#include <memory>

namespace app {

using namespace ui;

class CloseFileCommand : public Command {
public:
  CloseFileCommand()
    : Command("CloseFile",
              "Close File",
              CmdUIOnlyFlag) {
  }

  Command* clone() const override { return new CloseFileCommand(*this); }

protected:

  bool onEnabled(Context* context) override {
    Workspace* workspace = UIContext::instance()->activeWorkspace();
    WorkspaceView* view = workspace->activeView();
    return (view != nullptr);
  }

  void onExecute(Context* context) override {
    Workspace* workspace = UIContext::instance()->activeWorkspace();
    WorkspaceView* view = workspace->activeView();
    if (view)
      workspace->closeView(view, false);
  }
};

class CloseAllFilesCommand : public Command {
public:
  CloseAllFilesCommand()
    : Command("CloseAllFiles",
              "Close All Files",
              CmdRecordableFlag) {
    m_quitting = false;
  }

  Command* clone() const override { return new CloseAllFilesCommand(*this); }

protected:

  void onLoadParams(const Params& params) override {
    m_quitting = params.get_as<bool>("quitting");
  }

  void onExecute(Context* context) override {
    auto* uiContext = UIContext::instance();
    std::vector<doc::Document*> documents;
    for (auto* document : uiContext->documents())
      documents.push_back(document);

    for (auto* document : documents) {
      while (auto* docView = uiContext->getFirstDocumentView(document)) {
        auto* workspace = uiContext->workspaceFor(docView);
        if (!workspace || !workspace->closeView(docView, m_quitting))
          return;
      }
    }
  }

private:
  bool m_quitting;
};

Command* CommandFactory::createCloseFileCommand()
{
  return new CloseFileCommand;
}

Command* CommandFactory::createCloseAllFilesCommand()
{
  return new CloseAllFilesCommand;
}

} // namespace app
