// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/browser/application_event_router.h"

#include <algorithm>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "content/public/browser/browser_thread.h"
#include "xwalk/application/browser/application_event_registrar.h"
#include "xwalk/application/browser/application_process_manager.h"
#include "xwalk/application/browser/application_service.h"
#include "xwalk/application/browser/application_system.h"
#include "xwalk/application/common/application_file_util.h"

using content::BrowserThread;

namespace xwalk {
namespace application {

Event::Event(
    const std::string& event_name, scoped_ptr<base::ListValue> event_args)
  : name(event_name),
    args(event_args.Pass()) {
  DCHECK(this->args.get());
  printf("%s %d\n", __FUNCTION__, __LINE__);
}

Event::~Event() {
  printf("%s %d %s\n", __FUNCTION__, __LINE__, name.c_str());
}

EventHandler::EventHandler(const std::string& event_name,
                           const std::string& app_id,
                           const EventHandlerCallback& callback,
                           const base::WeakPtr<AppEventRegistrar>& registrar,
                           int priority)
  : event_name_(event_name),
    app_id_(app_id),
    callback_(callback),
    weak_owner_(registrar),
    priority_(priority) {
  CHECK(base::MessageLoop::current());
  loop_ = base::MessageLoopProxy::current();
  printf("%s %d %s\n", __FUNCTION__, __LINE__, event_name_.c_str());
}

EventHandler::~EventHandler() {
  printf("%s %d %p\n", __FUNCTION__, __LINE__, this);
}

void EventHandler::Notify(
    scoped_refptr<Event> event, const EventHandlerFinishCallback& finish_cb) {
  loop_->PostTask(FROM_HERE,
                  base::Bind(&EventHandler::Run,
                             base::Unretained(this), event, finish_cb));
}

void EventHandler::Run(
    scoped_refptr<Event> event, const EventHandlerFinishCallback& finish_cb) {
  if (weak_owner_) {
  printf("%s %d\n", __FUNCTION__, __LINE__);
    callback_.Run(event, finish_cb);
  } else {
  printf("%s %d\n", __FUNCTION__, __LINE__);
    finish_cb.Run();
  }
}

bool EventHandler::Equals(const EventHandler& handler) const {
  return event_name_ == handler.event_name_ &&
         app_id_ == handler.app_id_ &&
         priority_ == handler.priority_ &&
         callback_.Equals(handler.callback_);
}

ApplicationEventRouter::ApplicationEventRouter(ApplicationSystem* system)
  : system_(system) {
}

ApplicationEventRouter::~ApplicationEventRouter() {
}

bool ApplicationEventRouter::DispatchOnLaunchedEvent(
    const std::string& app_id) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  scoped_refptr<Event> event(
      new Event("app.onLaunched", args.Pass()));
  DispatchEventToApp(app_id, event);
  return true;
}

bool ApplicationEventRouter::DispatchOnLaunchedEvent(
    const base::FilePath& path) {
  ApplicationService* service = system_->application_service();
  scoped_refptr<const Application> app = service->Load(path);
  return DispatchOnLaunchedEvent(app->ID());
}

bool ApplicationEventRouter::DispatchEventToApp(
    const std::string& app_id, scoped_refptr<Event> event) {
  bool ret = true;
  ApplicationService* service = system_->application_service();
  std::set<std::string> main_events = GetMainEvents(app_id);

  if (main_events.find(event->name) != main_events.end() ||
      event->name == "app.onLaunched") {
    if (!service->GetRunningApplication() ||
        (main_observer_.get() && !main_observer_->IsStopLoading())) {
      if (lazy_events_.empty())
        ret = service->Launch(app_id);
      lazy_events_.push_back(event);
      return ret;
    }
  }

  // TODO(xiang): add process field to handler?
  if (service->GetRunningApplication())
    return ProcessEvent(app_id, event);
}

void ApplicationEventRouter::AddEventHandler(
    scoped_ptr<EventHandler> handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const std::string& event_name = handler->GetEventName();
  HandlerList& handlers = handlers_map_[event_name];

  HandlerList::iterator it = handlers.begin();
  for (; it != handlers.end(); it++) {
    if ((*it)->GetPriority() < handler->GetPriority())
      break;
  }
  handlers.insert(it, linked_ptr<EventHandler>(handler.release()));
}

void ApplicationEventRouter::RemoveEventHandler(
    scoped_ptr<EventHandler> handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const std::string& event_name = handler->GetEventName();
  HandlerList& handlers = handlers_map_[event_name];

  HandlerList::iterator it = handlers.begin();
  for (; it != handlers.end(); it++) {
    if ((*it)->Equals(*handler)) {
      // If the event handler is already running, then mark it for removing in
      // the finish callback.
      if (running_events_[event_name] == it) {
        removing_events_[event_name] = it;
        break;
      }
      handlers.erase(it);
      break;
    }
  }
#if 0
  for (HandlerList::iterator it = handlers.begin(); it != handlers.end(); it++)
    printf("%d %p\n", __LINE__, it->owner);
#endif
}

void ApplicationEventRouter::OnMainDocumentCreated(
    const std::string& app_id, content::WebContents* contents) {
  main_observer_.reset(new MainDocObserver(app_id, contents, this));
}

std::set<std::string> ApplicationEventRouter::GetMainEvents(
    const std::string& app_id) {
  // TODO
  std::set<std::string> events;
  return events;
}

void ApplicationEventRouter::SetMainEvents(
    const std::string& app_id, const std::set<std::string>& events) {
  // TODO
}

bool ApplicationEventRouter::ProcessEvent(
    const std::string& app_id, scoped_refptr<Event> event) {
  HandlerList* handlers = NULL;
  EventHandlerMap::iterator it = handlers_map_.find(event->name);
  if (it != handlers_map_.end())
    handlers = &(it->second);

  if (!handlers || (handlers && handlers->empty())) {
    LOG(INFO) << "No registered handler to handle event:" << event->name;
    return false;
  }
  
  HandlerList::iterator cur_handler_it;
  cur_handler_it = handlers->begin();
  running_events_[event->name] = cur_handler_it;
  (*cur_handler_it)->Notify(event,
                            base::Bind(&ApplicationEventRouter::OnFinishHandler,
                                       base::Unretained(this), app_id, event));
  return true;
}

void ApplicationEventRouter::OnFinishHandler(
    const std::string& app_id, scoped_refptr<Event> event) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(
            &ApplicationEventRouter::OnFinishHandler,
            base::Unretained(this), app_id, event));
      return;
  }

  HandlerList& handlers = handlers_map_[event->name];
  HandlerList::iterator cur_handler_it = running_events_[event->name];
  HandlerList::iterator tmp_it = cur_handler_it++;
  if (removing_events_.find(event->name) != removing_events_.end() &&
      tmp_it == removing_events_[event->name]) {
    handlers.erase(removing_events_[event->name]);
    removing_events_.erase(event->name);
  }

  if (cur_handler_it == handlers.end()) {
    running_events_.erase(event->name);
  } else {
    running_events_[event->name] = cur_handler_it;
    (*cur_handler_it)->Notify(event,
                              base::Bind(
                                &ApplicationEventRouter::OnFinishHandler,
                                base::Unretained(this), app_id, event));
  }
}

ApplicationEventRouter::MainDocObserver::MainDocObserver(
    const std::string& app_id,
    content::WebContents* contents,
    ApplicationEventRouter* router)
  : app_id_(app_id),
    contents_(contents),
    router_(router),
    did_stop_loading_(false) {
  content::WebContentsObserver::Observe(contents_);
}

void ApplicationEventRouter::MainDocObserver::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  did_stop_loading_ = true;
  if (!router_->lazy_events_.empty()) {
    LazyEvents::iterator it = router_->lazy_events_.begin();
    for (; it != router_->lazy_events_.end(); it++) {
      router_->ProcessEvent(app_id_, *it);
    }
    router_->lazy_events_.clear();
  }
}

}  // namespace application
}  // namespace xwalk
