// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_APPLICATION_BROWSER_APPLICATION_EVENT_ROUTER_H_
#define XWALK_APPLICATION_BROWSER_APPLICATION_EVENT_ROUTER_H_

#include <map>
#include <list>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_contents_observer.h"

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
class AppEventRegistrar;

class Event : public base::RefCounted<Event> {
 public:
  Event(const std::string& event_name, scoped_ptr<base::ListValue> event_args);

  // The event to dispatch.
  std::string name;
  
  // Arguments to send to the event handler.
  scoped_ptr<base::ListValue> args;

 private:
  friend class base::RefCounted<Event>;
  ~Event();
};

typedef base::Callback<void()> EventHandlerFinishCallback;
typedef base::Callback<void(scoped_refptr<Event>,
    const EventHandlerFinishCallback&)> EventHandlerCallback;

class EventHandler {
 public:
  // TODO(xiang): an event handler with an empty |app_id| string will handle the
  // specified |event_name| for all existing applications.
  EventHandler(const std::string& event_name,
               const std::string& app_id,
               const EventHandlerCallback& callback,
               const base::WeakPtr<AppEventRegistrar>& registrar,
               int priority = 100);

  ~EventHandler();

  // Called by EventRouter to dispatch the task to its creation thread.
  void Notify(scoped_refptr<Event> event,
              const EventHandlerFinishCallback& finish_cb);

  // A wrapper to run the callback_ task.
  void Run(scoped_refptr<Event> event,
           const EventHandlerFinishCallback& finish_cb);

  bool Equals(const EventHandler& handler) const;

  const std::string& GetEventName() const { return event_name_; }
  const std::string& GetAppID() const { return app_id_; }
  int GetPriority() const { return priority_; }

 private:
  // The event name this handler want to listen to.
  std::string event_name_;

  // The application who owns the handler.
  std::string app_id_;

  // Weak pointer to AppEventRegistrar which owns this handler.
  base::WeakPtr<AppEventRegistrar> weak_owner_;

  // The handler's priority to handle such event when multiple handlers are
  // registered to the same event. Handlers will be executed in descending order
  // of the priorities, the default value will be 100.
  int priority_;

  // Where the real event processing happens.
  EventHandlerCallback callback_;

  // Saved message loop proxy to post the task.
  scoped_refptr<base::MessageLoopProxy> loop_;
};

class ApplicationEventRouter {
 public:
  explicit ApplicationEventRouter(ApplicationSystem* system);
  ~ApplicationEventRouter();

  // Send "app.onLaunched" event to the specified application. 
  bool DispatchOnLaunchedEvent(const std::string& app_id);
  bool DispatchOnLaunchedEvent(const base::FilePath& path);

  // Send arbitrary event to |app_id| application. If the application is not
  // launched yet or not finish loading the main document the |event| will be
  // queued for later processing.
  bool DispatchEventToApp(
      const std::string& app_id, scoped_refptr<Event> event);

  // Add event handler, can be called on any thread. 
  void AddEventHandler(scoped_ptr<EventHandler> handler);
  
  // Remove event handler, can be called on any thread. 
  void RemoveEventHandler(scoped_ptr<EventHandler> handler);

  // Create the corresponding MainDocObserver when main document is created. 
  void OnMainDocumentCreated(
      const std::string& app_id, content::WebContents* contents);

  // Get/set events registered in main document.
  // TODO(xiang): save these events at app loading/installation time.
  std::set<std::string> GetMainEvents(const std::string& app_id);
  void SetMainEvents(
      const std::string& app_id, const std::set<std::string>& events);

 private:
  friend class ApplicationEventRouterTest;
  FRIEND_TEST_ALL_PREFIXES(ApplicationEventRouterTest, EventHandler);
  FRIEND_TEST_ALL_PREFIXES(
      ApplicationEventRouterTest, EventHandlerOnDifferentThread);

  class MainDocObserver : public content::WebContentsObserver {
   public:
    MainDocObserver(const std::string& app_id,
                    content::WebContents* contents,
                    ApplicationEventRouter* router);

    bool IsStopLoading() const { return did_stop_loading_; }
   
    // Implement content::WebContentsObserver. 
    virtual void DidStopLoading(
        content::RenderViewHost* render_view_host) OVERRIDE;

   private:
    std::string app_id_;
    content::WebContents* contents_;
    ApplicationEventRouter* router_;
    bool did_stop_loading_;
  };

  // Start dispatching |event| for app with |app_id| to registered handlers.
  bool ProcessEvent(
      const std::string& app_id, scoped_refptr<Event> event);

  // Will be used by event handler to prceed event processing.
  void OnFinishHandler(
      const std::string& app_id, scoped_refptr<Event> event);

  typedef std::list<linked_ptr<EventHandler> > HandlerList;
  typedef std::map<std::string, HandlerList> EventHandlerMap;
  typedef std::vector<scoped_refptr<Event> > LazyEvents;
  typedef std::map<std::string, HandlerList::iterator> HandlerItMap;

  EventHandlerMap handlers_map_;
  LazyEvents lazy_events_;
  HandlerItMap running_events_;
  HandlerItMap removing_events_;

  scoped_ptr<MainDocObserver> main_observer_;
  
  ApplicationSystem* system_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationEventRouter);
};

}  // namespace application
}  // namespace xwalk
#endif  // XWALK_APPLICATION_BROWSER_APPLICATION_EVENT_ROUTER_H_
