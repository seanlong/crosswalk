// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/browser/application_process_manager.h"

#include <string>

#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "xwalk/application/common/application_manifest_constants.h"
#include "xwalk/application/common/application_messages.h"
#include "xwalk/application/common/constants.h"
#include "xwalk/runtime/browser/runtime.h"
#include "xwalk/runtime/browser/runtime_context.h"
#include "net/base/net_util.h"

using content::WebContents;
using xwalk::Runtime;
using xwalk::RuntimeContext;

namespace xwalk {
namespace application {

ApplicationProcessManager::ApplicationProcessManager(
    RuntimeContext* runtime_context)
    : runtime_context_(runtime_context),
      main_runtime_(NULL),
      weak_ptr_factory_(this) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
      content::NotificationService::AllBrowserContextsAndSources());
}

ApplicationProcessManager::~ApplicationProcessManager() {
}

bool ApplicationProcessManager::LaunchApplication(
        RuntimeContext* runtime_context,
        const Application* application) {
  if (RunMainDocument(application))
    return true;
  // NOTE: For now we allow launching a web app from a local path. This may go
  // away at some point.
  return RunFromLocalPath(application);
}

bool ApplicationProcessManager::RunMainDocument(
    const Application* application) {
  const Manifest* manifest = application->GetManifest();
  const base::DictionaryValue* dict = NULL;
  if (!manifest->GetDictionary(application_manifest_keys::kAppMainKey, &dict))
    return false;

  GURL url;
  std::string main_source;
  const base::ListValue* main_scripts = NULL;
  manifest->GetString(application_manifest_keys::kAppMainSourceKey,
      &main_source);
  manifest->GetList(application_manifest_keys::kAppMainScriptsKey,
      &main_scripts);

  if (!main_source.empty() && (main_scripts && main_scripts->GetSize()))
    LOG(WARNING) << "An app should not has more than one main document.";

  if (!main_source.empty()) {
    url = application->GetResourceURL(main_source);
  } else if (main_scripts && main_scripts->GetSize()) {
    // When no main.source is defined but main.scripts are, we implicitly create
    // a main document.
    url = application->GetResourceURL(kGeneratedMainDocumentFilename);
  } else {
    LOG(WARNING) << "The app.main field doesn't contain a valid main document.";
    return false;
  }

  main_runtime_ = Runtime::Create(runtime_context_, url);
  application_ = application;
  return true;
}

bool ApplicationProcessManager::RunFromLocalPath(
    const Application* application) {
  const Manifest* manifest = application->GetManifest();
  std::string entry_page;
  if (manifest->GetString(application_manifest_keys::kLaunchLocalPathKey,
        &entry_page) && !entry_page.empty()) {
    GURL url = application->GetResourceURL(entry_page);
    if (url.is_empty()) {
      LOG(WARNING) << "Can't find a valid local path URL for app.";
      return false;
    }

    Runtime::CreateWithDefaultWindow(runtime_context_, url);
    application_ = application;
    return true;
  }

  return false;
}

void ApplicationProcessManager::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      if (!application_.get())
        break;
      content::RenderProcessHost* process =
        content::Source<content::RenderProcessHost>(source).ptr();
      ApplicationMsg_Launched_Params launched_application(application_.get());
      process->Send(new ApplicationMsg_Launched(launched_application));
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification type.";
  }
}

}  // namespace application
}  // namespace xwalk
