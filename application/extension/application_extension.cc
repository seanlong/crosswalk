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
#include "xwalk/application/common/application.h"
#include "xwalk/runtime/browser/runtime.h"

using content::BrowserThread;

using xwalk::application::ApplicationProcessManager;

// This will be generated from application_api.js.
extern const char kSource_application_api[];

namespace xwalk {

ApplicationExtension::ApplicationExtension(
    application::ApplicationSystem* application_system)
  : application_system_(application_system) {
  set_name("xwalk.app");
}

const char* ApplicationExtension::GetJavaScriptAPI() {
  return kSource_application_api;
}

XWalkExtensionInstance* ApplicationExtension::CreateInstance() {
  return new ApplicationExtensionInstance(application_system_);
}

ApplicationExtensionInstance::ApplicationExtensionInstance(
    application::ApplicationSystem* application_system)
  : application_system_(application_system),
    handler_(this),
    weak_ptr_factory_(this) {
  application_ =
      application_system_->application_service()->GetRunningApplication();
  event_callback_ = base::Bind(
      &ApplicationExtensionInstance::OnEventReceived, base::Unretained(this));
  
  handler_.Register(
      "getManifest",
      base::Bind(&ApplicationExtensionInstance::OnGetManifest,
                 base::Unretained(this)));
  handler_.Register(
      "getMainDocumentID",
      base::Bind(&ApplicationExtensionInstance::OnGetMainDocumentID,
                 base::Unretained(this)));
  handler_.Register(
      "registerEvent",
      base::Bind(&ApplicationExtensionInstance::OnRegisterEvent,
                 base::Unretained(this)));
  handler_.Register(
      "unregisterEvent",
      base::Bind(&ApplicationExtensionInstance::OnUnregisterEvent,
                 base::Unretained(this)));
  handler_.Register(
      "dispatchEventFinish",
      base::Bind(&ApplicationExtensionInstance::OnDispatchEventFinish,
                 base::Unretained(this)));
}

ApplicationExtensionInstance::~ApplicationExtensionInstance() {
  std::set<std::string>::iterator it = registered_events_.begin();
  while (it != registered_events_.end()) {
    const std::string& event_name = *it;
    ++it;
    RemoveEventHandler(event_name);
  }
}

void ApplicationExtensionInstance::HandleMessage(scoped_ptr<base::Value> msg) {
  handler_.HandleMessage(msg.Pass());
}

void ApplicationExtensionInstance::OnGetManifest(
    scoped_ptr<XWalkExtensionFunctionInfo> info) {
  base::DictionaryValue** manifest_data = new DictionaryValue*(NULL);
  BrowserThread::PostTaskAndReply(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ApplicationExtensionInstance::GetManifest,
                 base::Unretained(this),
                 base::Unretained(manifest_data)),
      base::Bind(&ApplicationExtensionInstance::PostManifest,
                 base::Unretained(this),
                 base::Passed(&info),
                 base::Owned(manifest_data)));
}

void ApplicationExtensionInstance::OnGetMainDocumentID(
    scoped_ptr<XWalkExtensionFunctionInfo> info) {
  int* main_routing_id = new int(MSG_ROUTING_NONE);
  BrowserThread::PostTaskAndReply(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ApplicationExtensionInstance::GetMainDocumentID,
                 base::Unretained(this),
                 base::Unretained(main_routing_id)),
      base::Bind(&ApplicationExtensionInstance::PostMainDocumentID,
                 base::Unretained(this),
                 base::Passed(&info),
                 base::Owned(main_routing_id)));
}

void ApplicationExtensionInstance::OnRegisterEvent(
    scoped_ptr<XWalkExtensionFunctionInfo> info) {
  std::string event_name;
  if (info->arguments()->GetSize() != 1 ||
      !info->arguments()->GetString(0, &event_name))
    return;

  std::set<std::string>::iterator it = registered_events_.find(event_name);
  if (it == registered_events_.end())
    AddEventHandler(event_name);
}

void ApplicationExtensionInstance::OnUnregisterEvent(
    scoped_ptr<XWalkExtensionFunctionInfo> info) {
  std::string event_name;
  if (info->arguments()->GetSize() != 1 ||
      !info->arguments()->GetString(0, &event_name))
    return;

  std::set<std::string>::iterator it = registered_events_.find(event_name);
  if (it != registered_events_.end())
    RemoveEventHandler(event_name);
}

void ApplicationExtensionInstance::AddEventHandler(
  const std::string& event_name) {
  application::ApplicationEventRouter* router =
      application_system_->event_router();
  
  registered_events_.insert(event_name);
  scoped_ptr<EventHandler> handler(new EventHandler(
        event_name, application_->ID(),event_callback_,
        weak_ptr_factory_.GetWeakPtr()));
  router->AddEventHandler(handler.Pass());

  //TODO(xiang): save event registered from main document.
}

void ApplicationExtensionInstance::RemoveEventHandler(
    const std::string& event_name) {
  application::ApplicationEventRouter* router =
      application_system_->event_router();
  
  scoped_ptr<EventHandler> handler(new EventHandler(
        event_name, application_->ID(), event_callback_,
        weak_ptr_factory_.GetWeakPtr()));
  router->RemoveEventHandler(handler.Pass());

  registered_events_.erase(event_name);

  // TODO(xiang): delete event unregistered from main document.
}

void ApplicationExtensionInstance::OnDispatchEventFinish(
    scoped_ptr<XWalkExtensionFunctionInfo> info) {
  std::string event_name;
  if (info->arguments()->GetSize() != 1 ||
      !info->arguments()->GetString(0, &event_name))
    return;

  pending_callbacks_[event_name].Run();
  pending_callbacks_.erase(event_name);
}

void ApplicationExtensionInstance::GetManifest(
    base::DictionaryValue** manifest_data) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const application::ApplicationService* service =
    application_system_->application_service();
  const application::Application* app = service->GetRunningApplication();
  if (app)
    *manifest_data = app->GetManifest()->value()->DeepCopy();

#if 0
//Test only
  application::ApplicationEventRouter* router =
      application_system_->event_router();
  scoped_ptr<base::ListValue> args(new base::ListValue());
  router->DispatchEventToApp(application_->ID(), new Event("onTestEvent", args.Pass()));
#endif
}

void ApplicationExtensionInstance::PostManifest(
    scoped_ptr<XWalkExtensionFunctionInfo> info,
    base::DictionaryValue** manifest_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_ptr<base::ListValue> results(new base::ListValue());
  if (*manifest_data)
    results->Append(*manifest_data);
  else
    // Return an empty dictionary value when there's no valid manifest data.
    results->Append(new base::DictionaryValue());
  info->PostResult(results.Pass());
}

void ApplicationExtensionInstance::GetMainDocumentID(int* main_routing_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const application::ApplicationProcessManager* pm =
    application_system_->process_manager();
  const Runtime* runtime = pm->GetMainDocumentRuntime();
  if (runtime)
    *main_routing_id = runtime->web_contents()->GetRoutingID();
}

void ApplicationExtensionInstance::PostMainDocumentID(
    scoped_ptr<XWalkExtensionFunctionInfo> info, int* main_routing_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_ptr<base::ListValue> results(new base::ListValue());
  results->AppendInteger(*main_routing_id);
  info->PostResult(results.Pass());
}

void ApplicationExtensionInstance::OnEventReceived(
    scoped_refptr<Event> event,
    const EventHandler::HandlerFinishCallback& callback) {
  scoped_ptr<base::DictionaryValue> msg(new base::DictionaryValue());
  msg->Set("cmd", new StringValue("dispatchEvent"));
  msg->Set("event", new StringValue(event->name));

  PostMessageToJS(msg.PassAs<base::Value>()); 
  pending_callbacks_[event->name] = callback;
}

}  // namespace xwalk
