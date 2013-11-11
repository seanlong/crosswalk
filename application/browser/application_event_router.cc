// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/browser/application_event_router.h"

#include <algorithm>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents_observer.h"
#include "xwalk/application/browser/application_process_manager.h"
#include "xwalk/application/browser/application_service.h"
#include "xwalk/application/browser/application_system.h"
#include "xwalk/application/common/application_file_util.h"

using content::BrowserThread;

namespace xwalk {
namespace application {

scoped_refptr<Event> Event::CreateEvent(
    const std::string& event_name, scoped_ptr<base::ListValue> event_args) {
  return scoped_refptr<Event>(new Event(event_name, event_args.Pass()));
}

Event::Event(
    const std::string& event_name, scoped_ptr<base::ListValue> event_args)
  : name(event_name),
    args(event_args.Pass()) {
  DCHECK(this->args.get());
}

Event::~Event() {
}

AppEventObserver::AppEventObserver(ApplicationEventRouter* router,
                                   const std::string& app_id) 
  : router_(router),
    app_id_(app_id) {
}

AppEventObserver::~AppEventObserver() {
  DetachFromAllEvent();
}

void AppEventObserver::AttachToEvent(const std::string& event_name,
                                     int priority) {
  if (std::find(events_.begin(), events_.end(), event_name) != events_.end()) {
    DLOG(WARNING) << "Observer " << this
                  << " already attached to event:" << event_name;
    return;
  }
  router_->AttachObserver(event_name, priority, this);
  events_.push_back(event_name);
}

void AppEventObserver::DetachFromEvent(const std::string& event_name) {
  std::vector<std::string>::iterator it = std::find(
      events_.begin(), events_.end(), event_name);
  if (it == events_.end()) {
    DLOG(WARNING) << "Observer " << this
                  << " doesn't attached to event:" << event_name;
    return;
  }
  router_->DetachObserver(event_name, this);
  events_.erase(it);
}

void AppEventObserver::DetachFromAllEvent() {
  router_->DetachObserver(this);
  events_.clear();
}
  
ApplicationEventRouter::ObserverWrapper::ObserverWrapper(
    AppEventObserver* observer, int priority)
  : observer_(observer),
    priority_(priority) {
  CHECK(base::MessageLoopProxy::current());
  loop_ = base::MessageLoopProxy::current();
}

void ApplicationEventRouter::ObserverWrapper::Notify(
    scoped_refptr<Event> event, const EventFinishCallback& finish_cb) {
  loop_->PostTask(FROM_HERE,
                  base::Bind(&ObserverWrapper::Run,
                             base::Unretained(this), event, finish_cb));
}

bool ApplicationEventRouter::ObserverWrapper::operator==(
    const ObserverWrapper& other) const {
  if (this == &other)
    return true;
  // A observer can't attach to an event more than once. So only observer
  // pointer wil be compared.
  return observer_ == other.observer_;
}

void ApplicationEventRouter::ObserverWrapper::Run(
    scoped_refptr<Event> event, const EventFinishCallback& finish_cb) {
  if (observer_) {
   observer_->Observe(event, finish_cb);
  } else {
   finish_cb.Run();
  }
}

ApplicationEventRouter::PerAppRouter::PerAppRouter(const std::string& app_id)
    : app_id_(app_id),
      is_launched_(false) {
}

ApplicationEventRouter::PerAppRouter::~PerAppRouter() {
}

void ApplicationEventRouter::PerAppRouter::SetMainEvents(
    const std::set<std::string>& events) {
  NOTIMPLEMENTED();
}

bool ApplicationEventRouter::PerAppRouter::ContainsMainEvent(
    const std::string event) {
  NOTIMPLEMENTED();
  return false;
}

void ApplicationEventRouter::PerAppRouter::AttachObserverToEvent(
    const std::string& event_name,
    const linked_ptr<ObserverWrapper>& observer) {
  base::AutoLock locker(lock_);
  ObserverList& observers = observers_[event_name];
  ObserverList::iterator it = observers.begin();
  // TODO(xiang): use std::find instead.
  for (; it != observers.end(); ++it) {
    if (**it == *observer)
      break;
  }
  if (it != observers.end()) {
    LOG(WARNING) << "Observer already exist for application(" << app_id_
                 << ") with event(" << event_name << ").";
    return;
  }

  it = observers.begin();
  for (; it != observers.end(); ++it) {
    if ((*it)->GetPriority() < observer->GetPriority())
      break;
  }
  observers.insert(it, observer);
}

void ApplicationEventRouter::PerAppRouter::DetachObserverFromEvent(
    const std::string& event_name,
    const ObserverWrapper& observer) {
  base::AutoLock locker(lock_);
  if (observers_.find(event_name) == observers_.end()) {
    LOG(WARNING) << "Can't find attached observer for application(" << app_id_
                 << ") with event(" << event_name << ")."; //DLOG
    return;
  }

  ObserverList& observers = observers_[event_name];
  ObserverList::iterator it = observers.begin();
  for (; it != observers.end(); ++it) {
    if (!(**it == observer))
      continue;
    // If the event handler is already running, then mark it for removing in
    // the finish callback.
    if (running_observers_.find(event_name) != running_observers_.end() &&
        running_observers_[event_name] == *it) {
      removing_observers_[event_name] = *it;
      // The observer pointer is invalid now.
      (*it)->Reset();
      return;
    }
    observers.erase(it);
    return;
  }
}

bool ApplicationEventRouter::PerAppRouter::HasLazyEvent() const {
  return !lazy_events_.empty();
}

void ApplicationEventRouter::PerAppRouter::PushLazyEvent(
    scoped_refptr<Event> event) {
  // It will only be accessed in UI thread, so no lock needed.
  lazy_events_.push_back(event);
}

void ApplicationEventRouter::PerAppRouter::ProcessLazyEvents() {
  if (lazy_events_.empty())
    return;

  EventVector::iterator it = lazy_events_.begin();
  for (; it != lazy_events_.end(); ++it) {
    DispatchEvent(*it);
  }
  lazy_events_.clear();
}

void ApplicationEventRouter::PerAppRouter::DispatchEvent(
    scoped_refptr<Event> event) {
  base::AutoLock locker(lock_);
  if (observers_.find(event->name) == observers_.end() ||
      observers_[event->name].empty()) {
    LOG(INFO) << "No registered handler to handle event:" << event->name;
    return;
  }

  ObserverList& observers = observers_[event->name];
  // If the application is in the middle of processing of an event with the same
  // name. Then the new incoming event will be queued and dispatched until its
  // previous ones are finished.
  if (running_observers_.find(event->name) != running_observers_.end()) {
    pending_events_[event->name].push_back(event);
    return;
  }

  linked_ptr<ObserverWrapper>& observer = *(observers.begin());
  running_observers_[event->name] = observer;
  observer->Notify(
      event,
      base::Bind(&ApplicationEventRouter::PerAppRouter::OnFinishObserver,
                 base::Unretained(this), event));
}

void ApplicationEventRouter::PerAppRouter::OnFinishObserver(
    scoped_refptr<Event> event) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ApplicationEventRouter::PerAppRouter::OnFinishObserver,
                     base::Unretained(this), event));
      return;
  }

  base::AutoLock locker(lock_);
  
  const std::string& event_name = event->name;
  DCHECK(observers_.find(event_name) != observers_.end());
  DCHECK(!observers_[event_name].empty());
  
  // Finish an observer which is removed.
  ObserverList& observers = observers_[event_name];
  ObserverList::iterator cur_it = std::find(
      observers.begin(), observers.end(), running_observers_[event_name]);
  DCHECK(cur_it != observers.end());
  ObserverList::iterator tmp_it = cur_it++;
  if (removing_observers_.find(event_name) != removing_observers_.end() &&
      *tmp_it == removing_observers_[event_name]) {
    observers.erase(tmp_it);
    removing_observers_.erase(event_name);
  }

  if (cur_it == observers.end()) {
    if (pending_events_.find(event_name) == pending_events_.end() ||
        pending_events_[event_name].empty()) {
      running_observers_.erase(event_name);
      return;
    }

    // We have pending events, then processing it from the beginning.
    event = pending_events_[event_name][0];
    pending_events_[event_name].pop_front();
    cur_it = observers.begin();
  }

  running_observers_[event_name] = *cur_it;
  (*cur_it)->Notify(
      event,
      base::Bind(&ApplicationEventRouter::PerAppRouter::OnFinishObserver,
                 base::Unretained(this), event));
}

void ApplicationEventRouter::PerAppRouter::SetLaunched() {
  is_launched_ = true;
}

bool ApplicationEventRouter::PerAppRouter::IsLaunched() const {
  NOTIMPLEMENTED();
  return is_launched_;
}

class ApplicationEventRouter::MainDocObserver
    : public content::WebContentsObserver {
 public:
  MainDocObserver(const std::string& app_id,
                  content::WebContents* contents,
                  ApplicationEventRouter* router);

  // Implement content::WebContentsObserver.
  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;

 private:
  std::string app_id_;
  content::WebContents* contents_;
  ApplicationEventRouter* router_;
};

ApplicationEventRouter::MainDocObserver::MainDocObserver(
    const std::string& app_id,
    content::WebContents* contents,
    ApplicationEventRouter* router)
  : app_id_(app_id),
    contents_(contents),
    router_(router) {
  DCHECK(router_ && contents_);
  content::WebContentsObserver::Observe(contents_);
}

void ApplicationEventRouter::MainDocObserver::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  linked_ptr<PerAppRouter> app_router = router_->GetAppRouter(app_id_);
  if (!app_router.get())
    return;
 
  app_router->SetLaunched();
  app_router->ProcessLazyEvents();
  router_->main_observers_.erase(app_id_);
}

ApplicationEventRouter::ApplicationEventRouter(ApplicationSystem* system)
  : system_(system) {
}

ApplicationEventRouter::~ApplicationEventRouter() {
}

void ApplicationEventRouter::OnAppLoaded(const std::string& app_id) {
  base::AutoLock locker(routers_lock_);
  app_routers_.insert(std::make_pair(
        app_id, linked_ptr<PerAppRouter>(new PerAppRouter(app_id))));
}

void ApplicationEventRouter::DispatchEventToApp(
    const std::string& app_id, scoped_refptr<Event> event) {
  ApplicationService* service = system_->application_service();
  linked_ptr<PerAppRouter> app_router = GetAppRouter(app_id);
  if (!app_router.get())
    return;

  if (app_router->ContainsMainEvent(event->name) ||
      event->name == "app.onLaunched") {
    if (!app_router->IsLaunched()) {
      if (!app_router->HasLazyEvent() && !service->Launch(app_id))
          LOG(WARNING) << "Fail to launch application:" << app_id;
      else
        app_router->PushLazyEvent(event);
      return;
    }
  }

  if (app_router->IsLaunched())
    app_router->DispatchEvent(event);
  else
    LOG(WARNING) << "Event: " << event->name
                 << " send to terminated application:" << app_id;
}

void ApplicationEventRouter::AttachObserver (
    const std::string& event_name,
    int priority,
    AppEventObserver* observer) {
  linked_ptr<ObserverWrapper> ob_wrapper(
      new ObserverWrapper(observer, priority));

  if (observer->IsShared()) {
    AttachSharedObserver(event_name, ob_wrapper);
    return;
  }

  AttachObserverToAppEvent(observer->GetAppId(), event_name, ob_wrapper);
}

void ApplicationEventRouter::AttachSharedObserver(
    const std::string& event_name,
    const linked_ptr<ObserverWrapper>& observer) {
  {
    base::AutoLock locker(shared_observers_lock_);
    shared_observers_[event_name].push_back(observer);
  }
  
  std::vector<std::string> apps;
  {
    base::AutoLock locker(routers_lock_);
    AppRouterMap::iterator it = app_routers_.begin();
    for (; it != app_routers_.end(); ++it)
      apps.push_back(it->first);
  }

  for (int i = 0; i < apps.size(); ++i)
    AttachObserverToAppEvent(apps[i], event_name, observer);
}

void ApplicationEventRouter::AttachObserverToAppEvent(
    const std::string& app_id,
    const std::string& event_name,
    const linked_ptr<ObserverWrapper>& observer) {
  linked_ptr<PerAppRouter> app_router = GetAppRouter(app_id);
  if (app_router.get())
    app_router->AttachObserverToEvent(event_name, observer);
}

void ApplicationEventRouter::DetachObserver(
    const std::string& event_name,
    AppEventObserver* observer) {
  ObserverWrapper ob_wrapper(observer);
  if (observer->IsShared()) {
    DetachSharedObserver(event_name, ob_wrapper);
    return;
  }

  DetachObserverFromAppEvent(observer->GetAppId(), event_name, ob_wrapper);
}

void ApplicationEventRouter::DetachObserver(AppEventObserver* observer) {
  ObserverWrapper ob_wrapper(observer);
  const std::vector<std::string>& events = observer->GetEvents();
  std::vector<std::string>::const_iterator it = events.begin();
  if (observer->IsShared()) {
    for (; it != events.end(); ++it)
      DetachSharedObserver(*it, ob_wrapper);
    return;
  }

  for (; it != events.end(); ++it)
    DetachObserverFromAppEvent(observer->GetAppId(), *it, ob_wrapper);
}

void ApplicationEventRouter::DetachSharedObserver(
    const std::string& event_name,
    ObserverWrapper& observer) {
  {
    base::AutoLock locker(shared_observers_lock_);
    ObserverVector& observers = shared_observers_[event_name];
    ObserverVector::iterator it = observers.begin();
    for (; it != observers.end(); ++it) {
      if (**it == observer)
        break;
    }
    CHECK(it != observers.end());
    observers.erase(it);
  }
  
  std::vector<std::string> apps;
  {
    base::AutoLock locker(routers_lock_);
    AppRouterMap::iterator it = app_routers_.begin();
    for (; it != app_routers_.end(); ++it)
      apps.push_back(it->first);
  }
  for (int i = 0; i != apps.size(); ++i)
    DetachObserverFromAppEvent(apps[i], event_name, observer);
}

void ApplicationEventRouter::DetachObserverFromAppEvent(
    const std::string& app_id,
    const std::string& event_name,
    ObserverWrapper& observer) {
  linked_ptr<PerAppRouter> app_router = GetAppRouter(app_id);
  if (app_router.get())
    app_router->DetachObserverFromEvent(event_name, observer);
}

linked_ptr<ApplicationEventRouter::PerAppRouter>
ApplicationEventRouter::GetAppRouter(const std::string& app_id) {
  base::AutoLock locker(routers_lock_);
  AppRouterMap::iterator it = app_routers_.find(app_id);
  if (it == app_routers_.end()) {
    LOG(WARNING) << "Application " << app_id << " router not found."; // DLOG
    return linked_ptr<PerAppRouter>(NULL);
  }

  return it->second;
}


void ApplicationEventRouter::OnMainDocumentCreated(
    const std::string& app_id, content::WebContents* contents) {
  main_observers_[app_id] = linked_ptr<MainDocObserver>(
      new MainDocObserver(app_id, contents, this));
}

const std::set<std::string> ApplicationEventRouter::GetAppMainEvents(
    const std::string& app_id) {
  NOTIMPLEMENTED();
  return std::set<std::string>();
}

void ApplicationEventRouter::SetAppMainEvents(
    const std::string& app_id, const std::set<std::string>& events) {
  NOTIMPLEMENTED();
}

}  // namespace application
}  // namespace xwalk
