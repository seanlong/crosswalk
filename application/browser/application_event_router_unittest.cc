// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "xwalk/application/browser/application_event_registrar.h"
#include "xwalk/application/browser/application_event_router.h"
#include "xwalk/application/browser/application_system.h"
#include "xwalk/runtime/browser/runtime_context.h"

namespace xwalk {
namespace application {

namespace {

const char* kMockEvent0 = "MOCK_EVENT_0";
const char* kMockEvent1 = "MOCK_EVENT_1";
const char* kMockAppId = "mock_app";

struct MockEventProcessor {
  explicit MockEventProcessor(ApplicationEventRouter* router)
    : registrar(router),
      async_handler_count(0),
      sync_handler_count(0),
      async_task_completion(true, false) {
  }

  void HandleEventSync(scoped_refptr<Event> event,
                       const EventHandlerFinishCallback& callback) {
    if (first_handler.empty())
      first_handler = "sync_handler";
    ++sync_handler_count;
    callback.Run();
  }

  void HandleEventAsync(scoped_refptr<Event> event,
                        const EventHandlerFinishCallback& callback) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MockEventProcessor::DelayedTask,
                   base::Unretained(this), callback),
        base::TimeDelta::FromMilliseconds(50));
  }

  void HandleEventAsyncWithCond(scoped_refptr<Event> event,
                                const EventHandlerFinishCallback& callback) {
    async_task_completion.Reset();
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MockEventProcessor::DelayedTaskWithCond,
                   base::Unretained(this), callback),
        base::TimeDelta::FromMilliseconds(50));
  }

  int async_handler_count;
  int sync_handler_count;
  std::string first_handler;
  AppEventRegistrar registrar;
  base::WaitableEvent async_task_completion;

 private:
  void DelayedTask(const EventHandlerFinishCallback& callback) {
    if (first_handler.empty())
      first_handler = "async_handler";
    ++async_handler_count;
    callback.Run();
  }

  void DelayedTaskWithCond(const EventHandlerFinishCallback& callback) {
    ++async_handler_count;
    async_task_completion.Signal();
    callback.Run();
  }
};

// Helper class to manipulate the mock processor in "test_thread".
class TestThread : public base::Thread {
 public:
  TestThread()
    : base::Thread("test_thread") {
  }

  virtual ~TestThread() {
    Stop();
  }

  void CreateEventProcessor(ApplicationEventRouter* router) {
    base::WaitableEvent completion(true, false);
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&TestThread::DoCreateEventProcessor,
                   base::Unretained(this), router, &completion));
    completion.Wait();
  }

  void RemoveEventProcessor() {
    base::WaitableEvent completion(true, false);
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&TestThread::DoRemoveEventProcessor,
                   base::Unretained(this), &completion));
    completion.Wait();
  }

  void RemoveAsyncHandler() {
    base::WaitableEvent completion(true, false);
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&TestThread::DoRemoveAsyncHandler,
                   base::Unretained(this), &completion));
    completion.Wait();
  }

  void WaitAsyncHandler() {
    processor_->async_task_completion.Wait();
  }

  int GetAsyncHandlerCallCount() const {
    return processor_->async_handler_count;
  }

  int GetSyncHandlerCallCount() const {
    return processor_->sync_handler_count;
  }

 private:
  void DoCreateEventProcessor(ApplicationEventRouter* router,
                              base::WaitableEvent* completion) {
    processor_ = new MockEventProcessor(router);
    processor_->registrar.Add(kMockEvent0, kMockAppId,
                              base::Bind(&MockEventProcessor::HandleEventSync,
                                         base::Unretained(processor_)));
    saved_callback_ = base::Bind(&MockEventProcessor::HandleEventAsyncWithCond,
                                 base::Unretained(processor_));
    processor_->registrar.Add(kMockEvent1, kMockAppId, saved_callback_);
    completion->Signal();
  }

  void DoRemoveEventProcessor(base::WaitableEvent* completion) {
    delete processor_;
    completion->Signal();
  }

  void DoRemoveAsyncHandler(base::WaitableEvent* completion) {
    processor_->registrar.Remove(kMockEvent1, kMockAppId, saved_callback_);
    completion->Signal();
  }

  MockEventProcessor* processor_;
  EventHandlerCallback saved_callback_;
};

}  // namespace

class ApplicationEventRouterTest : public testing::Test {
 public:
  ApplicationEventRouterTest() {}

  virtual void SetUp() OVERRIDE {
    runtime_context_.reset(new xwalk::RuntimeContext);
    system_.reset(new ApplicationSystem(runtime_context_.get()));
    router_ = system_->event_router();
  }

  void SendEvent0ToRouter() {
    scoped_refptr<Event> event = new Event(
        kMockEvent0, scoped_ptr<base::ListValue>(new base::ListValue()));
    router_->ProcessEvent(kMockAppId, event);
  }

  void SendEvent1ToRouter() {
    scoped_refptr<Event> event = new Event(
        kMockEvent1, scoped_ptr<base::ListValue>(new base::ListValue()));
    router_->ProcessEvent(kMockAppId, event);
  }

 protected:
  ApplicationEventRouter* router_;

 private:
  scoped_ptr<xwalk::RuntimeContext> runtime_context_;
  scoped_ptr<ApplicationSystem> system_;
  
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(ApplicationEventRouterTest, EventHandler) {
  MockEventProcessor processor(router_);
  processor.registrar.Add(kMockEvent0, kMockAppId,
                          base::Bind(
                              &MockEventProcessor::HandleEventSync,
                              base::Unretained(&processor)));
  EXPECT_EQ(router_->handlers_map_.size(), 1);
  processor.registrar.Add(kMockEvent1, kMockAppId,
                          base::Bind(
                              &MockEventProcessor::HandleEventAsync,
                              base::Unretained(&processor)));
  EXPECT_EQ(router_->handlers_map_.size(), 2);
  EXPECT_EQ(router_->handlers_map_[kMockEvent0].size(), 1);
  EXPECT_EQ(router_->handlers_map_[kMockEvent1].size(), 1);

  // Test event handler registration with priority. 
  {
    MockEventProcessor processor(router_);
    processor.registrar.Add(kMockEvent0, kMockAppId,
                            base::Bind(&MockEventProcessor::HandleEventSync,
                                       base::Unretained(&processor)), 99);
    processor.registrar.Add(kMockEvent1, kMockAppId,
                            base::Bind(&MockEventProcessor::HandleEventSync,
                                       base::Unretained(&processor)), 101);
    EXPECT_EQ(router_->handlers_map_.size(), 2);
    EXPECT_EQ(router_->handlers_map_[kMockEvent1].size(), 2);
    EXPECT_EQ(
        (*router_->handlers_map_[kMockEvent1].begin())->GetPriority(), 101);
    EXPECT_EQ(router_->handlers_map_[kMockEvent0].size(), 2);
    EXPECT_EQ(
        (*router_->handlers_map_[kMockEvent0].rbegin())->GetPriority(), 99);
  }

  // Handlers should be removed when previous stack allocated processor deleted.
  EXPECT_EQ(router_->handlers_map_[kMockEvent0].size(), 1);
  EXPECT_EQ(router_->handlers_map_[kMockEvent1].size(), 1);

  // Test event dispatch. The async handler will be executed firstly.
  processor.registrar.Add(kMockEvent0,
                          kMockAppId,
                          base::Bind(&MockEventProcessor::HandleEventAsync,
                                     base::Unretained(&processor)), 101);
  SendEvent0ToRouter();
  for (int i = 0; i < 10; i++) {
    if (processor.async_handler_count == 1)
      break;
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
    base::MessageLoop::current()->RunUntilIdle();
  }

  EXPECT_EQ(processor.async_handler_count, 1);
  EXPECT_EQ(processor.sync_handler_count, 1);
  EXPECT_EQ(processor.first_handler, "async_handler");
}

TEST_F(ApplicationEventRouterTest, EventHandlerOnDifferentThread) {
  TestThread test_thread;
  test_thread.Start();
  test_thread.CreateEventProcessor(router_);

  // Wait for ApplicationEventRouter handling the handler add task.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(router_->handlers_map_.size(), 2);
  EXPECT_EQ(router_->handlers_map_[kMockEvent0].size(), 1);
  EXPECT_EQ(router_->handlers_map_[kMockEvent1].size(), 1);

  // Send events to router, check both sync/async handler are called.
  SendEvent0ToRouter();
  SendEvent1ToRouter();
  test_thread.WaitAsyncHandler();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(test_thread.GetAsyncHandlerCallCount(), 1);
  EXPECT_EQ(test_thread.GetSyncHandlerCallCount(), 1);

  // Remove async handler when it's running. After the async handler is complete
  // then the handler should be remove from router.
  SendEvent1ToRouter();
  test_thread.RemoveAsyncHandler();
  test_thread.WaitAsyncHandler();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(router_->handlers_map_[kMockEvent1].size(), 0);

  // Remove processor will remove all registered handlers.
  test_thread.RemoveEventProcessor();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(router_->handlers_map_[kMockEvent0].size(), 0);
  // Must not crash.
  SendEvent0ToRouter();
}

}  // namespace application
}  // namespace xwalk
