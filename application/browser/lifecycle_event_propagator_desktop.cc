// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/browser/lifecycle_event_propagator_desktop.h"

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "xwalk/application/browser/application_event_router.h"
#include "xwalk/application/browser/application_system.h"
#include "xwalk/runtime/browser/runtime.h"
#include "xwalk/runtime/browser/runtime_context.h"
#include "xwalk/runtime/browser/runtime_registry.h"

using content::WebContents;

namespace xwalk {
namespace application {

LifecycleEventPropagator::LifecycleEventPropagator(
    xwalk::RuntimeContext* runtime_context)
  : runtime_context_(runtime_context),
    state_(RUNNING) {
  //FIXME the router_ can't initial from runtime_context yet.
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
}

void LifecycleEventPropagator::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED: {
      // TODO in shared browser process mode, we should know which web contents
      // belongs to which application.
      router_ = runtime_context_->GetApplicationSystem()->event_router();
      bool visible = *content::Details<bool>(details).ptr();
      // If one window is become visible and previous state is suspended, then
      // trigger resume event.
      if (visible && state_ == SUSPENDED) {
        state_ = RUNNING;
        scoped_ptr<ApplicationEvent> event(new ApplicationEvent(
              "RESUME", scoped_ptr<base::ListValue>(new base::ListValue())));
        router_->QueueEvent(event.Pass());
        break;
      }
      const xwalk::RuntimeList& runtimes = RuntimeRegistry::Get()->runtimes();
      xwalk::RuntimeList::const_iterator it = runtimes.begin();
      for (; it != runtimes.end(); it++) {
        if ((*it)->window() && !(*it)->window()->IsMinimized())
          break;
      }
      // If all windows are become invisible then trigger SUSPEND event.
      if (it == runtimes.end()) {
        // TODO suspend cancel implementation.
        state_ = SUSPENDED;
        scoped_ptr<ApplicationEvent> event(new ApplicationEvent(
              "SUSPEND", scoped_ptr<base::ListValue>(new base::ListValue())));
        router_->QueueEvent(event.Pass());
      }
      break;
    }

    case content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED: {
      // TODO in shared browser process mode, we should know which web contents
      // belongs to which application.
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      ApplicationProcessManager* pm =
        runtime_context_->GetApplicationSystem()->process_manager();
      Runtime* main_runtime = pm->GetMainDocumentRuntime();
      CHECK(web_contents &&
            main_runtime &&
            web_contents != main_runtime->web_contents());
      const xwalk::RuntimeList& runtimes = RuntimeRegistry::Get()->runtimes();
      if (runtimes.size() == 2) {
        state_ = TERMINATING;
        scoped_ptr<ApplicationEvent> event(new ApplicationEvent(
              "TERMINATING", scoped_ptr<base::ListValue>(new base::ListValue())));
        router_->QueueEvent(event.Pass());
      }
      break;
    }

    default:
      NOTREACHED() << L"Received unexpected notification";
      break;
  }
}

}
}
