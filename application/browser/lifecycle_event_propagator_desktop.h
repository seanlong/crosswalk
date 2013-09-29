// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_APPLICATION_BROWSER_LIFECYCLE_EVENT_PROPAGATOR_H_
#define XWALK_APPLICATION_BROWSER_LIFECYCLE_EVENT_PROPAGATOR_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace xwalk {
class RuntimeContext;
}

namespace xwalk {
namespace application {

class ApplicationEventRouter;

class LifecycleEventPropagator : public content::NotificationObserver {
 public:
  LifecycleEventPropagator(xwalk::RuntimeContext* runtime_context);

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  enum STATE {
    RUNNING = 0,
    SUSPENDED,
    TERMINATING
  };

  content::NotificationRegistrar registrar_;
  xwalk::RuntimeContext* runtime_context_;
  ApplicationEventRouter* router_;
  STATE state_;
};

}
}

#endif
