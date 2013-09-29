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

class LifecycleEventObserver : public ApplicationEventObserver,
                              public content::BrowserMessageFilter {
 public:
  LifecycleEventObserver(
      ApplicationEventRouter* router, RuntimeContext* context);

  void RegisterEventHandlers() OVERRIDE;

  // content::BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  void OnHide(const linked_ptr<ApplicationEvent>& event,
      const base::Callback<void()>& callback);
  void OnShow(const linked_ptr<ApplicationEvent>& event,
      const base::Callback<void()>& callback);

  void OnShowAck();

  xwalk::RuntimeContext* runtime_context_;
  base::Callback<void()> showack_callback_;
};

}
}

#endif
