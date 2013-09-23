// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/extension/application_extension.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "xwalk/application/browser/application_process_manager.h"
#include "xwalk/application/browser/application_system.h"
#include "xwalk/runtime/browser/runtime.h"
#include "xwalk/runtime/browser/runtime_context.h"

using content::BrowserThread;

// This will be generated from application_api.js.
extern const char kSource_application_api[];

namespace xwalk {
using extensions::XWalkExtension;
using extensions::XWalkExtensionInstance;
using extensions::XWalkInternalExtension;
using extensions::XWalkInternalExtensionInstance;

ApplicationExtension::ApplicationExtension(RuntimeContext* runtime_context)
  : runtime_context_(runtime_context) {
  set_name("xwalk.application");
}

const char* ApplicationExtension::GetJavaScriptAPI() {
  return kSource_application_api;
}

XWalkExtensionInstance* ApplicationExtension::CreateInstance() {
  return new ApplicationExtensionInstance(runtime_context_);
}

ApplicationExtensionInstance::ApplicationExtensionInstance(
    RuntimeContext* runtime_context)
  : XWalkInternalExtensionInstance(),
    runtime_context_(runtime_context),
    main_routing_id_(MSG_ROUTING_NONE) {
  RegisterFunction("getMainDocumentID",
      &ApplicationExtensionInstance::OnGetMainDocumentID);
}

void ApplicationExtensionInstance::OnGetMainDocumentID(
    const std::string& function_name,
    const std::string& callback_id,
    base::ListValue* args) {
  BrowserThread::PostTaskAndReply(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ApplicationExtensionInstance::GetMainDocumentID,
                 base::Unretained(this)),
      base::Bind(&ApplicationExtensionInstance::PostMainDocumentID,
                 base::Unretained(this), callback_id));
}

void ApplicationExtensionInstance::GetMainDocumentID() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const application::ApplicationProcessManager* pm =
    runtime_context_->GetApplicationSystem()->process_manager();
  const Runtime* runtime = pm->GetMainDocumentRuntime();
  if (runtime)
    main_routing_id_ = runtime->web_contents()->GetRoutingID();
}

void ApplicationExtensionInstance::PostMainDocumentID(
    const std::string& callback_id) {
  scoped_ptr<base::ListValue> results(new base::ListValue());
  results->AppendInteger(main_routing_id_);
  PostResult(callback_id, results.Pass());
}

}  // namespace xwalk
