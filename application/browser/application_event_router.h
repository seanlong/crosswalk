// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_APPLICATION_BROWSER_APPLICATION_EVENT_ROUTER_H_
#define XWALK_APPLICATION_BROWSER_APPLICATION_EVENT_ROUTER_H_

#include <string>
#include <deque>
#include <map>
#include <list>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"

namespace xwalk {
class RuntimeContext;
}

namespace xwalk {
namespace application {

class ApplicationEventObserver;

struct ApplicationEvent {
  std::string name;
  scoped_ptr<base::ListValue> args;

  ApplicationEvent(const std::string& event_name,
                   scoped_ptr<base::ListValue> event_args);
  ~ApplicationEvent();
};

class ApplicationEventRouter {
 public:
  typedef base::Callback<void(const linked_ptr<ApplicationEvent>&,
      const base::Callback<void()>&)> EventHandlerCallback;
  typedef std::deque<linked_ptr<ApplicationEvent> > EventQueue;
  typedef std::list<ApplicationEventObserver*> ObserverList;
  
  class ObserverRegistrar {
   public:
    ObserverRegistrar(ApplicationEventRouter* router);
    ~ObserverRegistrar();

    void Add(ApplicationEventObserver* observer);
    void Remove();
    void AddEventHandler(const std::string& event_name,
                         int priority,
                         const EventHandlerCallback& callback);

   private:
    ApplicationEventRouter* router_;
    ApplicationEventObserver* owner_;
  };

  ApplicationEventRouter();
  ~ApplicationEventRouter() {}

  void AddObserver(ApplicationEventObserver* observer);
  void RemoveObserver(ApplicationEventObserver* observer);
  void QueueEvent(scoped_ptr<ApplicationEvent> event);
  void DispatchEvent(linked_ptr<ApplicationEvent>& event);

 private:
  FRIEND_TEST_ALL_PREFIXES(ApplicationEventRouterTest, ObserverRegistration);
  FRIEND_TEST_ALL_PREFIXES(ApplicationEventRouterTest, EventDispatch);

  struct EventHandler {
    ApplicationEventObserver* owner;
    int priority;
    EventHandlerCallback callback;

    EventHandler(ApplicationEventObserver*, int, const EventHandlerCallback&);
  };

  void AddEventHandler(ApplicationEventObserver* observer,
                       const std::string& event_name,
                       int priority,
                       const EventHandlerCallback& callback);
  void RemoveEventHandler(ApplicationEventObserver* observer,
                          const std::string& event_name,
                          const EventHandlerCallback& callback);
  void RemoveObserverEventHandlers(ApplicationEventObserver* observer);
  void ProceedHandler();

  typedef std::list<EventHandler> HandlerList;
  typedef std::map<const std::string, HandlerList> EventHandlerMap;

  EventHandlerMap handlers_map_;
  ObserverList observers_;
  EventQueue event_queue_;

  // Save intermediate states for handlers' asynchronous processing.
  linked_ptr<ApplicationEvent> cur_event_;
  HandlerList* cur_handlers_;
  HandlerList::iterator cur_handler_it_;
  
  DISALLOW_COPY_AND_ASSIGN(ApplicationEventRouter);
};

class ApplicationEventObserver {
 public:
  explicit ApplicationEventObserver(ApplicationEventRouter* router);
  ~ApplicationEventObserver();
  
  virtual void RegisterEventHandlers() = 0;

 protected:
  ApplicationEventRouter::ObserverRegistrar registrar_;
 
 private: 
  DISALLOW_COPY_AND_ASSIGN(ApplicationEventObserver);
};

}
}
#endif
