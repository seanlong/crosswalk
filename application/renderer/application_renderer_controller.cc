// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/renderer/application_renderer_controller.h"

#include <string>

#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "xwalk/application/common/application_messages.h"

namespace xwalk {
namespace application {

// TODO remove this later...
namespace {
class RenderViewEventCallbacker : public content::RenderViewVisitor {
 public:
  bool Visit(content::RenderView* render_view) OVERRIDE;
};

bool RenderViewEventCallbacker::Visit(content::RenderView* render_view) {
  std::string js = "if (xwalk.application.onShow) "
                   "xwalk.application.onShow();";
  string16 js16;
  UTF8ToUTF16(js.c_str(), js.length(), &js16);
  string16 frame_xpath;
  render_view->EvaluateScript(frame_xpath, js16, 0, false);
  return true;
}

}

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
    IPC_MESSAGE_HANDLER(ApplicationMsg_Show, OnShow)
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

void ApplicationRendererController::OnShow() {
  //TODO make event object, register/callback framework.
  
  //For now we just visit each render view's context and do callback if exist.
  RenderViewEventCallbacker callbacker;
  content::RenderView::ForEach(&callbacker);
  content::RenderThread::Get()->Send(new ApplicationMsg_ShowAck());
}

const Application*
ApplicationRendererController::GetRunningApplication() const {
  return application_.get();
}

}  // namespace application
}  // namespace xwalk

