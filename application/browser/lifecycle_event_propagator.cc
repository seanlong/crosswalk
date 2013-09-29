// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/browser/lifecycle_event_propagator.h"

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "xwalk/application/browser/application_event_router.h"
#include "xwalk/application/browser/application_system.h"
#include "xwalk/runtime/browser/runtime.h"
#include "xwalk/runtime/browser/runtime_context.h"
#include "xwalk/runtime/browser/runtime_registry.h"

namespace xwalk {
namespace application {

LifecycleEventPropagator::LifecycleEventPropagator(
    xwalk::RuntimeContext* runtime_context)
  : runtime_context_(runtime_context) {
  //FIXME the router_ can't initial from runtime_context yet.
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
      content::NotificationService::AllBrowserContextsAndSources());
}

void LifecycleEventPropagator::Observe(int type,
                                       const content::NotificationSource& source,
                                       const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED: {
      //TODO which web contents belongs to the application
      router_ = runtime_context_->GetApplicationSystem()->event_router();
      bool visible = *content::Details<bool>(details).ptr();
      content::WebContents* contents = content::Source<content::WebContents>(source).ptr();
      const xwalk::RuntimeList& runtimes = RuntimeRegistry::Get()->runtimes();
      xwalk::RuntimeList::const_iterator it = runtimes.begin();
      
      for (; it != runtimes.end(); it++) {
        if ((*it)->window()) {
          if ((visible && (*it)->window()->IsMinimized()) ||
              (!visible && !(*it)->window()->IsMinimized()))
          break;
        }
      }
      // if one window is become visible then send SHOW event,
      // if all windows are become invisible then send HIDE event.
      if (it == runtimes.end()) {
        const char* event_name = visible ? "SHOW" : "HIDE";
        scoped_ptr<ApplicationEvent> event(new ApplicationEvent(
              event_name, scoped_ptr<base::ListValue>(new base::ListValue())));
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
