// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/location.h"
#include "base/test/test_timeouts.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "xwalk/application/browser/application_event_router.h"

namespace xwalk {
namespace application {

namespace {

const char* kMockEvent0 = "MOCK_EVENT_0";
const char* kMockEvent1 = "MOCK_EVENT_1";

class FirstMockEventObserver : public ApplicationEventObserver {
 public:
  FirstMockEventObserver(ApplicationEventRouter* router)
    : ApplicationEventObserver(router) {
    registrar_.Add(this);
  }

  void RegisterEventHandlers() OVERRIDE {
    registrar_.AddEventHandler(kMockEvent0, 1,
        base::Bind(
          &FirstMockEventObserver::OnMockEvents, base::Unretained(this)));
    registrar_.AddEventHandler(kMockEvent1, 1,
        base::Bind(
          &FirstMockEventObserver::OnMockEvents, base::Unretained(this)));
  }
  
  int GetHandlerCallCount(const std::string& event_name) {
    EventHandlerMap::iterator it = handlers_call_count_.find(event_name);
    if (it == handlers_call_count_.end())
      return 0;
    return it->second;
  }

 private:
  typedef std::map<const std::string, int> EventHandlerMap;
  
  void OnMockEvents(const linked_ptr<ApplicationEvent>& event,
      const base::Callback<void()>& callback) {
    handlers_call_count_[event->name] = GetHandlerCallCount(event->name) + 1;
    callback.Run();
  }

  EventHandlerMap handlers_call_count_;
};

class SecondMockEventObserver : public ApplicationEventObserver {
 public:
  SecondMockEventObserver(ApplicationEventRouter* router)
    : ApplicationEventObserver(router) {
    registrar_.Add(this);
  }

  void RegisterEventHandlers() OVERRIDE {
    registrar_.AddEventHandler(kMockEvent0, 0,
        base::Bind(
          &SecondMockEventObserver::OnMockEvent1, base::Unretained(this)));
    registrar_.AddEventHandler(kMockEvent1, 2,
        base::Bind(
          &SecondMockEventObserver::OnMockEvent2, base::Unretained(this)));
  }
  
  int GetHandlerCallCount(const std::string& event_name) {
    EventHandlerMap::iterator it = handlers_call_count_.find(event_name);
    if (it == handlers_call_count_.end())
      return 0;
    return it->second;
  }

  const std::string& GetLastEventName() {
    return last_event_name_;
  }

 private:
  typedef std::map<const std::string, int> EventHandlerMap;

  void OnMockEvent1(const linked_ptr<ApplicationEvent>& event,
     const base::Callback<void()>& callback) {
    handlers_call_count_[event->name] = GetHandlerCallCount(event->name) + 1;
    last_event_name_ = event->name;
    callback.Run();
  }

  void OnMockEvent2(const linked_ptr<ApplicationEvent>& event,
      const base::Callback<void()>& callback) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SecondMockEventObserver::MockEvent2Callback,
                   base::Unretained(this),
                   event->name,
                   callback),
        TestTimeouts::tiny_timeout());
  }

  void MockEvent2Callback(const std::string& event_name,
                          const base::Callback<void()>& callback) {
    handlers_call_count_[event_name] = GetHandlerCallCount(event_name) + 1;
    last_event_name_ = event_name;
    callback.Run();
    base::MessageLoop::current()->QuitNow();
  }

  EventHandlerMap handlers_call_count_;
  std::string last_event_name_;
};

}

class ApplicationEventRouterTest : public testing::Test {
 public:
  ApplicationEventRouterTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
  }

 protected:
  virtual void SetUp() OVERRIDE {
  }
  
  content::TestBrowserThreadBundle thread_bundle_;
};

// Test observer/handler construction/destruction/registration...
TEST_F(ApplicationEventRouterTest, ObserverRegistration) {
  ApplicationEventRouter router;
  FirstMockEventObserver* observer_1 = new FirstMockEventObserver(&router);
  EXPECT_EQ(router.observers_.size(), 1);
  EXPECT_EQ(router.handlers_map_.size(), 2);
  EXPECT_EQ(router.handlers_map_[kMockEvent0].size(), 1);

  {
    SecondMockEventObserver observer_2(&router);
    EXPECT_EQ(router.handlers_map_[kMockEvent0].size(), 2);
    EXPECT_EQ(router.handlers_map_[kMockEvent1].size(), 2);
    // Handler with higher priority will closer to the list front.
    const ApplicationEventRouter::HandlerList& handlers =
      router.handlers_map_[kMockEvent1];
    ApplicationEventRouter::HandlerList::const_iterator it = handlers.begin();
    EXPECT_EQ(it->owner, &observer_2);
    EXPECT_EQ(it->priority, 2);
    it++;
    EXPECT_EQ(it->owner, observer_1);
    EXPECT_EQ(it->priority, 1);
  }

  EXPECT_EQ(router.observers_.size(), 1);
  EXPECT_EQ(router.handlers_map_.size(), 2);
  EXPECT_EQ(router.handlers_map_[kMockEvent1].size(), 1);

  // Both observer and handler lists should be cleared.
  delete observer_1;
  EXPECT_EQ(router.observers_.size(), 0);
  EXPECT_EQ(router.handlers_map_.size(), 0);
}

// Test ApplicatonEvent lifecycle and how it will be handled.
TEST_F(ApplicationEventRouterTest, EventDispatch) {
  ApplicationEventRouter router;
  FirstMockEventObserver observer_1(&router);
  scoped_ptr<ApplicationEvent> event_0(new ApplicationEvent(
      kMockEvent0, scoped_ptr<base::ListValue>(new base::ListValue())));
  router.QueueEvent(event_0.Pass());
  EXPECT_EQ(observer_1.GetHandlerCallCount(kMockEvent0), 1);
  
  SecondMockEventObserver observer_2(&router);
  scoped_ptr<ApplicationEvent> event_1(new ApplicationEvent(
      kMockEvent1, scoped_ptr<base::ListValue>(new base::ListValue())));
  event_0.reset(new ApplicationEvent(
      kMockEvent0, scoped_ptr<base::ListValue>(new base::ListValue())));
  router.QueueEvent(event_1.Pass());
  router.QueueEvent(event_0.Pass());
  EXPECT_EQ(router.event_queue_.size(), 2);
  
  base::MessageLoop::current()->Run();

  EXPECT_EQ(router.event_queue_.size(), 0);
  EXPECT_EQ(observer_1.GetHandlerCallCount(kMockEvent0), 2);
  EXPECT_EQ(observer_2.GetHandlerCallCount(kMockEvent0), 1);
  // MockEvent0 will be blocked until MockEvent1's work is finished.
  EXPECT_EQ(observer_2.GetLastEventName(), kMockEvent0);
}

}
}
