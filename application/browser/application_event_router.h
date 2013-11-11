// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_APPLICATION_BROWSER_APPLICATION_EVENT_ROUTER_H_
#define XWALK_APPLICATION_BROWSER_APPLICATION_EVENT_ROUTER_H_

#include <deque>
#include <map>
#include <list>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/values.h"

namespace base {
class MessageLoopProxy;
}

namespace content {
class WebContents;
}

namespace xwalk {
namespace application {

class ApplicationEventRouter;
class ApplicationSystem;

typedef base::Callback<void()> EventFinishCallback;

struct Event : public base::RefCounted<Event> {
  static scoped_refptr<Event> CreateEvent(
      const std::string& event_name, scoped_ptr<base::ListValue> event_args);

  // The event to dispatch.
  std::string name;

  // Arguments to send to the event handler.
  scoped_ptr<base::ListValue> args;

 private:
  friend class base::RefCounted<Event>;
  Event(const std::string& event_name, scoped_ptr<base::ListValue> event_args);
  ~Event();
};

// Base class to observe application events.
class AppEventObserver {
 public:
  // If |app_id| is empty then this observer will be shared by all applications.
  explicit AppEventObserver(ApplicationEventRouter* router,
                            const std::string& app_id = "");
  virtual ~AppEventObserver();

  void AttachToEvent(const std::string& event_name,
                     int priority = 100);
  void DetachFromEvent(const std::string& event_name);
  void DetachFromAllEvent();

  virtual void Observe(scoped_refptr<Event> event,
                       const EventFinishCallback& callback) = 0;

  const std::string& GetAppId() const { return app_id_; }
  
  bool IsShared() const { return app_id_.empty(); }

  const std::vector<std::string>& GetEvents() const { return events_; }

 private:
  ApplicationEventRouter* router_;
  std::string app_id_;
  std::vector<std::string> events_;
};

class ApplicationEventRouter {
 public:
  explicit ApplicationEventRouter(ApplicationSystem* system);
  ~ApplicationEventRouter();

  // Create app router when app is loaded.
  void OnAppLoaded(const std::string& app_id);
  // TODO(xiang): Destroy app router when app is unloaded.
  void OnAppUnloaded(const std::string& app_id) { NOTIMPLEMENTED(); }
  // TODO(xiang): attach existing shared observers when app is launched.
  void OnAppLaunched(const std::string& app_id) { NOTIMPLEMENTED(); }
  // TODO(xiang): detach all observers for an app when it's terminated.
  void OnAppTerminated(const std::string& app_id) { NOTIMPLEMENTED(); }
  
  // Send arbitrary event to |app_id| application. If the application is not
  // launched yet or not finish loading the main document the |event| will be
  // queued for later processing. Will only be called on UI thread.
  void DispatchEventToApp(const std::string& app_id,
                          scoped_refptr<Event> event);
  // TODO(xiang): Dispatch event to all loaded applications.
  void BroadcastEvent(scoped_refptr<Event> event) { NOTIMPLEMENTED(); }

  // Observer attach/detach functions, can be called on any thread.
  void AttachObserver(const std::string& event_name,
                      int priority,
                      AppEventObserver* observer);
  void DetachObserver(const std::string& event_name,
                      AppEventObserver* observer);
  void DetachObserver(AppEventObserver* observer);

  
  // Create the corresponding MainDocObserver when main document is created.
  void OnMainDocumentCreated(const std::string& app_id,
                             content::WebContents* contents);

  // Get/set events registered in main document.
  // TODO(xiang): save these events at app loading/installation time.
  const std::set<std::string> GetAppMainEvents(const std::string& app_id);
  void SetAppMainEvents(const std::string& app_id,
                        const std::set<std::string>& events);

 private:
  class ObserverWrapper {
   public:
    ObserverWrapper(AppEventObserver* observer, int priority = 100);
    ~ObserverWrapper() {}

    void Notify(scoped_refptr<Event> event,
                const EventFinishCallback& finish_cb);

    bool operator==(const ObserverWrapper& other) const;

    AppEventObserver* Get() { return observer_; }

    int GetPriority() const { return priority_; }

    void Reset() { observer_ = NULL; }

   private:
    void Run(scoped_refptr<Event> event, const EventFinishCallback& finish_cb);
    
    int priority_;
    AppEventObserver* observer_;
    scoped_refptr<base::MessageLoopProxy> loop_;
  };

  // Per application observer information container. It will be created when the 
  // application is loaded, and destructed when the applicaiton is unloaded.
  class PerAppRouter {
   public:
    explicit PerAppRouter(const std::string& app_id);
    ~PerAppRouter();

    void SetMainEvents(const std::set<std::string>& events);
    bool ContainsMainEvent(const std::string event);

    void AttachObserverToEvent(const std::string& event_name,
                               const linked_ptr<ObserverWrapper>& observer);
    void DetachObserverFromEvent(const std::string& event_name,
                                 const ObserverWrapper& observer);

    void DispatchEvent(scoped_refptr<Event> event);
    void OnFinishObserver(scoped_refptr<Event> event);

    bool HasLazyEvent() const;
    void PushLazyEvent(scoped_refptr<Event> event);
    void ProcessLazyEvents();

    // Returns true when application's main document or entry page is finished
    // loading.
    bool IsLaunched() const;
    void SetLaunched();

   private:
    FRIEND_TEST_ALL_PREFIXES(ApplicationEventRouterTest,
                             ObserverRegistration);
    typedef std::list<linked_ptr<ObserverWrapper> > ObserverList;
    // Key by event name.
    typedef std::map<std::string, ObserverList> ObserverListMap;
    // All attached observers.
    ObserverListMap observers_;

    // Key by event name. 
    typedef std::map<std::string, linked_ptr<ObserverWrapper> > ObserverMap;
    // Cached observers to handle removing an observer when it is running.
    ObserverMap running_observers_;
    ObserverMap removing_observers_;

    // Key by event name.
    typedef std::map<std::string, std::deque<scoped_refptr<Event> > >
        EventQueueMap;
    // Event queue when there's event with the same name is in processing.
    EventQueueMap pending_events_;

    typedef std::vector<scoped_refptr<Event> > EventVector;
    // Lazy events queued before application launched.
    EventVector lazy_events_;

    typedef std::set<std::string> EventSet;
    // Events registered in main document, will be filled when application is
    // loaded.
    EventSet main_events_;

    std::string app_id_;
    bool is_launched_;
    mutable base::Lock lock_;
  };

  friend class ApplicationEventRouterTest;
  FRIEND_TEST_ALL_PREFIXES(ApplicationEventRouterTest,
                           ObserverRegistration);
  FRIEND_TEST_ALL_PREFIXES(ApplicationEventRouterTest,
                           SharedObserverRegistration);
  FRIEND_TEST_ALL_PREFIXES(ApplicationEventRouterTest,
                           ObserverOnDifferentThread);

  class MainDocObserver;

  linked_ptr<PerAppRouter> GetAppRouter(const std::string& app_id);

  void AttachSharedObserver(const std::string& event_name,
                            const linked_ptr<ObserverWrapper>& observer);
 
  void AttachObserverToAppEvent(const std::string& app_id,
                                const std::string& event_name,
                                const linked_ptr<ObserverWrapper>& observer);
 
  void DetachSharedObserver(const std::string& event_name,
                            ObserverWrapper& observer);

  void DetachObserverFromAppEvent(const std::string& app_id,
                                  const std::string& event_name,
                                  ObserverWrapper& observer);

  typedef std::map<std::string, linked_ptr<PerAppRouter> > AppRouterMap;
  AppRouterMap app_routers_;
  mutable base::Lock routers_lock_;
  
  typedef std::vector<linked_ptr<ObserverWrapper> > ObserverVector;
  typedef std::map<std::string, ObserverVector> ObserverVectorMap;
  ObserverVectorMap shared_observers_;
  mutable base::Lock shared_observers_lock_;

  std::map<std::string, linked_ptr<MainDocObserver> > main_observers_;

  ApplicationSystem* system_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationEventRouter);
};

}  // namespace application
}  // namespace xwalk
#endif  // XWALK_APPLICATION_BROWSER_APPLICATION_EVENT_ROUTER_H_
