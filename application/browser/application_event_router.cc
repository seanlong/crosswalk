// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/browser/application_event_router.h"

#include <algorithm>

#include "base/bind.h"
#include "xwalk/application/browser/lifecycle_event_propagator.h"
#include "xwalk/runtime/browser/runtime.h"
#include "xwalk/runtime/browser/runtime_context.h"
#include "xwalk/runtime/browser/runtime_registry.h"


namespace xwalk {
namespace application {

////////////////////////////////////////////////////////////////////////////////
ApplicationEvent::ApplicationEvent(const std::string& event_name,
                                   scoped_ptr<base::ListValue> event_args)
  : name(event_name),
    args(event_args.Pass()) {
  DCHECK(this->args.get());
}

////////////////////////////////////////////////////////////////////////////////
ApplicationEventRouter::ObserverRegistrar::ObserverRegistrar(
    ApplicationEventRouter* router)
  : router_(router) {
}

void ApplicationEventRouter::ObserverRegistrar::Add(
    ApplicationEventObserver* observer) {
  owner_ = observer;
  router_->AddObserver(observer);
}

void ApplicationEventRouter::ObserverRegistrar::Remove() {
  router_->RemoveObserver(owner_);
}

void ApplicationEventRouter::ObserverRegistrar::AddEventHandler(
    const std::string& event_name,
    int priority,
    const EventHandlerCallback& callback) {
  router_->AddEventHandler(owner_, event_name, priority, callback);
}

ApplicationEventRouter::ObserverRegistrar::~ObserverRegistrar() {
  Remove();
}

ApplicationEventRouter::EventHandler::EventHandler(
    ApplicationEventObserver* observer,
    int handler_priority,
    const EventHandlerCallback& handler_callback)
  : owner(observer),
    priority(handler_priority),
    callback(handler_callback) {
}

////////////////////////////////////////////////////////////////////////////////
static LifecycleEventPropagator* g_lifecycle_propagator;

ApplicationEventRouter::ApplicationEventRouter(xwalk::RuntimeContext* context) {
  //TODO where to put these propagators and observers??
  g_lifecycle_propagator = new LifecycleEventPropagator(context);
}

void ApplicationEventRouter::AddObserver(
    ApplicationEventObserver* observer) {
  ObserverList::iterator it =
    find(observers_.begin(), observers_.end(), observer);
  if (it != observers_.end())
    return;
  
  observers_.push_back(observer);
  observer->RegisterEventHandlers();  
}

void ApplicationEventRouter::RemoveObserver(
    ApplicationEventObserver* observer) {
  ObserverList::iterator it =
    find(observers_.begin(), observers_.end(), observer);
  if (it == observers_.end())
    return;

  RemoveObserverEventHandlers(observer);
  observers_.remove(observer);
}

void ApplicationEventRouter::QueueEvent(scoped_ptr<ApplicationEvent> event) {
  event_queue_.push_back(linked_ptr<ApplicationEvent>(event.release()));
  // The router is idle, then let's start it.
  if (event_queue_.size() == 1) {
    DispatchEvent(event_queue_.front());
  }
}

void ApplicationEventRouter::DispatchEvent(
    linked_ptr<ApplicationEvent>& event) { //FIXME use 'const'?
  HandlerList* handlers = NULL;
  EventHandlerMap::iterator it = handlers_map_.find(event->name);
  if (it != handlers_map_.end())
    handlers = &(it->second);

  if (handlers && handlers->empty()) {
    LOG(WARNING) << "No registered handler to handle event:" << event->name;
    return;
  }

  // Run handlers with callback.
  cur_event_ = event;
  cur_handlers_ = handlers;
  cur_handler_it_ = handlers->begin();
  cur_handler_it_->callback.Run(event, base::Bind(
        &ApplicationEventRouter::ProceedHandler,
        base::Unretained(this)));
}

void ApplicationEventRouter::ProceedHandler() {
  // FIXME There could be race condition when a handler is removed during the
  // process. e.g., when current handler be erased, the iterator will become
  // invalid, then we need to update the iterator in the remove routine.
  cur_handler_it_++;
  if (cur_handler_it_ == cur_handlers_->end()) {
    cur_event_.reset();
    cur_handlers_ = NULL;
    event_queue_.pop_front();
    if (event_queue_.empty())
      return;
    DispatchEvent(event_queue_.front());
  } else {
    cur_handler_it_->callback.Run(cur_event_, base::Bind(
        &ApplicationEventRouter::ProceedHandler,
        base::Unretained(this)));
  }
}

void ApplicationEventRouter::AddEventHandler(ApplicationEventObserver* observer,
                                             const std::string& event_name,
                                             int priority,
                                             const EventHandlerCallback& callback) {
  ObserverList::iterator observer_it =
    std::find(observers_.begin(), observers_.end(), observer);
  if (observer_it == observers_.end()) {
    LOG(WARNING) << "Can't add event handler before the observer is registered.";
    return;
  }

  HandlerList& handlers = handlers_map_[event_name];
  HandlerList::iterator it = handlers.begin();
  for (; it != handlers.end(); it++) {
    if (it->priority > priority)
      break;
  } 
  EventHandler handler(observer, priority, callback);
  handlers.insert(it, handler);
}

void ApplicationEventRouter::RemoveEventHandler(
    ApplicationEventObserver* observer,
    const std::string& event_name,
    const EventHandlerCallback& callback) {
  ObserverList::iterator observer_it =
    find(observers_.begin(), observers_.end(), observer);
  if (observer_it == observers_.end()) {
    LOG(WARNING) << "Can't remove event handler before the observer is registered.";
    return;
  }

  HandlerList& handlers = handlers_map_[event_name];
  HandlerList::iterator it = handlers.begin();
  while (it != handlers.end()) {
    if (it->callback.Equals(callback))
      it = handlers.erase(it);
    else
      ++it;
  }
#if 0
  for (HandlerList::iterator it = handlers.begin(); it != handlers.end(); it++)
    printf("%d %p\n", __LINE__, it->owner);
#endif
}

void ApplicationEventRouter::RemoveObserverEventHandlers(
    ApplicationEventObserver* observer) {
  EventHandlerMap::iterator event_it = handlers_map_.begin();
  for (; event_it != handlers_map_.end(); event_it++) {
    HandlerList& handlers = event_it->second;
    HandlerList::iterator handler_it = handlers.begin();
    while (handler_it != handlers.end()) {
      if (handler_it->owner == observer)
        handler_it = handlers.erase(handler_it);
      else
        ++handler_it;
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
ApplicationEventObserver::ApplicationEventObserver(
    ApplicationEventRouter* router)
  : registrar_(router) {
}  

ApplicationEventObserver::~ApplicationEventObserver() {
}

}
}
