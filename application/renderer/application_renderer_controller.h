// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_APPLICATION_RENDERER_APPLICATION_RENDERER_CONTROLLER_H_
#define XWALK_APPLICATION_RENDERER_APPLICATION_RENDERER_CONTROLLER_H_

#include "content/public/renderer/render_process_observer.h"
#include "v8/include/v8.h"
#include "xwalk/application/common/application.h"

struct ApplicationMsg_Launched_Params;

namespace WebKit {
class WebFrame;
}

template <typename T> struct DefaultSingletonTraits;

namespace xwalk {
namespace application {

// This class will handle application control messages sent to the renderer.
class ApplicationRendererController : public content::RenderProcessObserver {
 public:
  static ApplicationRendererController* GetInstance();

  // RenderProcessObserver implementation:
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  const Application* GetRunningApplication() const;

 private:
  friend struct DefaultSingletonTraits<ApplicationRendererController>;

  ApplicationRendererController();
  virtual ~ApplicationRendererController();
  void OnLaunched(const ApplicationMsg_Launched_Params& launched_app);
  void OnSuspend();
  void OnResume();
  void OnTerminate();

  scoped_refptr<const Application> application_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationRendererController);
};

}  // namespace application
}  // namespace xwalk

#endif  // XWALK_APPLICATION_RENDERER_APPLICATION_RENDERER_CONTROLLER_H_

