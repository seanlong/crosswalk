// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/browser/lifecycle_event_observer.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "xwalk/application/browser/application_system.h"
#include "xwalk/application/browser/application_process_manager.h"
#include "xwalk/application/common/application_messages.h"
#include "xwalk/runtime/browser/runtime.h"
#include "xwalk/runtime/browser/runtime_context.h"
#include "xwalk/runtime/browser/runtime_registry.h"

using xwalk::Runtime;
using xwalk::RuntimeContext;
using xwalk::RuntimeRegistry;

namespace xwalk {
namespace application {

LifecycleEventObserver::LifecycleEventObserver(
    ApplicationEventRouter* router, RuntimeContext* context)
  : ApplicationEventObserver(router),
    runtime_context_(context) {
  registrar_.Add(this);
}

void LifecycleEventObserver::RegisterEventHandlers() {
  registrar_.AddEventHandler("HIDE", 0,
      base::Bind(&LifecycleEventObserver::OnHide, base::Unretained(this)));
  registrar_.AddEventHandler("SHOW", 0,
      base::Bind(&LifecycleEventObserver::OnShow, base::Unretained(this)));
}

bool LifecycleEventObserver::OnMessageReceived(const IPC::Message& message,
                                              bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(LifecycleEventObserver, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ApplicationMsg_ShowAck, OnShowAck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void LifecycleEventObserver::OnHide(const linked_ptr<ApplicationEvent>& event,
    const base::Callback<void()>& callback) {
  // At present all Runtime instances belong to one application.
#if 0
  const xwalk::RuntimeList& runtimes = RuntimeRegistry::Get()->runtimes();
  xwalk::RuntimeList::const_iterator it = runtimes.begin();
  for (; it != runtimes.end(); it++) {
    content::WebContents* contents = (*it)->web_contents();
    contents->WasHidden();
  }
#endif
  // We can do something here... e.g., purge V8 memory
  LOG(INFO) << "Let's do synchronous hidden work!"; 

  callback.Run();
}

void LifecycleEventObserver::OnShow(const linked_ptr<ApplicationEvent>& event,
    const base::Callback<void()>& callback) {
#if 0
  const xwalk::RuntimeList& runtimes = RuntimeRegistry::Get()->runtimes();
  xwalk::RuntimeList::const_iterator it = runtimes.begin();
  for (; it != runtimes.end(); it++) {
    content::WebContents* contents = (*it)->web_contents();
    contents->WasShown();
  }
#endif
  // Test notifies renderer this event and run the callback after ack
  LOG(INFO) << "Let's do asynchronous shown work!";
  ApplicationProcessManager* pm =
    runtime_context_->GetApplicationSystem()->process_manager();
  Runtime* main_runtime = pm->GetMainDocumentRuntime();
  content::RenderViewHost* rvh = main_runtime->web_contents()->GetRenderViewHost();
  rvh->Send(new ApplicationMsg_Show);
  showack_callback_ = callback;
}

void LifecycleEventObserver::OnShowAck() {
  LOG(INFO) << "Get show ack message from render.";
  showack_callback_.Run(); 
}

}
}
