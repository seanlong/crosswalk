// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_APPLICATION_BROWSER_APPLICATION_PROCESS_MANAGER_H_
#define XWALK_APPLICATION_BROWSER_APPLICATION_PROCESS_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "xwalk/application/common/application.h"

class GURL;

namespace xwalk {
class Runtime;
class RuntimeContext;
}

namespace xwalk {
namespace application {

class ApplicationHost;
class Manifest;

// This manages dynamic state of running applications. By now, it only launches
// one application, later it will manages all event pages' lifecycle.
class ApplicationProcessManager: public content::NotificationObserver {
 public:
  explicit ApplicationProcessManager(xwalk::RuntimeContext* runtime_context);
  ~ApplicationProcessManager();

  bool LaunchApplication(xwalk::RuntimeContext* runtime_context,
                         const Application* application);

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  bool RunMainDocument(const Application* application);
  bool RunFromLocalPath(const Application* application);

  xwalk::RuntimeContext* runtime_context_;
  content::NotificationRegistrar registrar_;
  scoped_refptr<const Application> application_;
  base::WeakPtrFactory<ApplicationProcessManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationProcessManager);
};

}  // namespace application
}  // namespace xwalk

#endif  // XWALK_APPLICATION_BROWSER_APPLICATION_PROCESS_MANAGER_H_
