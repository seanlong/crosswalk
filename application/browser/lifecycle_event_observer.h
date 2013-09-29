// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_APPLICATION_BROWSER_LIFECYCLE_EVENT_OBSERVER_H_
#define XWALK_APPLICATION_BROWSER_LIFECYCLE_EVENT_OBSERVER_H_

#include "base/callback.h"
#include "content/public/browser/browser_message_filter.h"
#include "xwalk/application/browser/application_event_router.h"

namespace xwalk {
class RuntimeContext;
}

namespace xwalk {
namespace application {

// Common lifecycle event observer implementation.
class LifecycleEventObserver : public ApplicationEventObserver,
                               public content::BrowserMessageFilter {
 public:
  LifecycleEventObserver(
      ApplicationEventRouter* router, xwalk::RuntimeContext* context);

  void RegisterEventHandlers() OVERRIDE;

  // content::BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  void OnEvent(const linked_ptr<ApplicationEvent>& event,
               const base::Callback<void()>& callback);

  void OnEventAck();

  xwalk::RuntimeContext* runtime_context_;
  base::Callback<void()> event_router_callback_;;
};

}
}

#endif
