// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/renderer/application_renderer_controller.h"

#include <string>

#include "base/memory/singleton.h"
#include "content/public/renderer/render_thread.h"
#include "xwalk/application/common/application_messages.h"

namespace xwalk {
namespace application {

ApplicationRendererController* ApplicationRendererController::GetInstance() {
  return Singleton<ApplicationRendererController>::get();
}

ApplicationRendererController::ApplicationRendererController() {
  content::RenderThread* thread = content::RenderThread::Get();
  thread->AddObserver(this);
}

ApplicationRendererController::~ApplicationRendererController() {
  content::RenderThread::Get()->RemoveObserver(this);
}

bool ApplicationRendererController::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ApplicationRendererController, message)
    IPC_MESSAGE_HANDLER(ApplicationMsg_Launched, OnLaunched)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ApplicationRendererController::OnLaunched(
    const ApplicationMsg_Launched_Params& launched_app) {
  std::string error;
  application_ = launched_app.ConvertToApplication(&error);
  if (!application_.get()) {
    LOG(ERROR) << "Can't convert application parameter from browser process.";
  }
}

const Application*
ApplicationRendererController::GetRunningApplication() const {
  return application_.get();
}

}  // namespace application
}  // namespace xwalk

