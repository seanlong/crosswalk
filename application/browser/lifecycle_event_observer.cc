// Copyright (c) 2013 Intel Corporation. All rights reserved.
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

namespace xwalk {
namespace application {

LifecycleEventObserver::LifecycleEventObserver(
    ApplicationEventRouter* router, RuntimeContext* context)
  : ApplicationEventObserver(router),
    runtime_context_(context) {
  registrar_.Add(this);
}

void LifecycleEventObserver::RegisterEventHandlers() {
  registrar_.AddEventHandler("SUSPEND", 10, //TODO max/min priority value.
      base::Bind(&LifecycleEventObserver::OnEvent, base::Unretained(this)));
  registrar_.AddEventHandler("RESUME", 10,
      base::Bind(&LifecycleEventObserver::OnEvent, base::Unretained(this)));
  registrar_.AddEventHandler("TERMINATE", 10,
      base::Bind(&LifecycleEventObserver::OnEvent, base::Unretained(this)));
}

bool LifecycleEventObserver::OnMessageReceived(const IPC::Message& message,
                                               bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(LifecycleEventObserver, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ApplicationMsg_SuspendAck, OnEventAck)
    IPC_MESSAGE_HANDLER(ApplicationMsg_ResumeAck, OnEventAck)
    IPC_MESSAGE_HANDLER(ApplicationMsg_TerminateAck, OnEventAck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void LifecycleEventObserver::OnEvent(const linked_ptr<ApplicationEvent>& event,
                                     const base::Callback<void()>& callback) {
  // We do common event handling here, e.g., trigger JS callback.
  ApplicationProcessManager* pm =
    runtime_context_->GetApplicationSystem()->process_manager();
  // TODO send routed message to main document as lifecycle event should only be
  // handled in main document.
  Runtime* main_runtime = pm->GetMainDocumentRuntime();
  content::RenderViewHost* rvh =
    main_runtime->web_contents()->GetRenderViewHost();
  IPC::Message* msg = NULL;
  if (event->name == "SUSPEND")
    msg = new ApplicationMsg_Suspend;
  else if (event->name == "RESUME")
    msg = new ApplicationMsg_Resume;
  else if (event->name == "TERMINATE")
    msg = new ApplicationMsg_Terminate;
  else
    LOG(ERROR) << "Unknown event received:" << event->name;

  if (msg) {
    event_router_callback_ = callback;
    rvh->Send(msg);
  }
}

void LifecycleEventObserver::OnEventAck() {
  event_router_callback_.Run();
}

}
}
