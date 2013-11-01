// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/browser/application_event_registrar.h"

#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace xwalk {
namespace application {

AppEventRegistrar::HandlerRecord::HandlerRecord(
    const std::string& event_name,
    const std::string& app_id,
    const EventHandlerCallback& callback)
  : event_name(event_name),
    app_id(app_id),
    callback(callback) {
}

AppEventRegistrar::HandlerRecord::~HandlerRecord() {
  printf("%s\n", __FUNCTION__);
}

bool
AppEventRegistrar::HandlerRecord::operator==(const HandlerRecord& other) const {
  return event_name == other.event_name &&
         app_id == other.app_id &&
         callback.Equals(other.callback);
}

AppEventRegistrar::AppEventRegistrar(ApplicationEventRouter* router)
  : router_(router),
    weak_ptr_factory_(this) {
}

AppEventRegistrar::~AppEventRegistrar() {
  RemoveAll();
  printf("%s\n", __FUNCTION__);
}

void AppEventRegistrar::Add(const std::string& event_name,
                            const std::string& app_id,
                            const EventHandlerCallback& callback) {
  linked_ptr<HandlerRecord> record(
      new HandlerRecord(event_name, app_id, callback));
  records_.push_back(record);

  scoped_ptr<EventHandler> handler(new EventHandler(
        event_name, app_id, callback, GetWeakPtr()));

  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ApplicationEventRouter::AddEventHandler,
                   base::Unretained(router_), base::Passed(&handler)));
  } else {
    router_->AddEventHandler(handler.Pass());
  }
}

void AppEventRegistrar::Remove(const std::string& event_name,
                               const std::string& app_id,
                               const EventHandlerCallback& callback) {
  HandlerRecord record(event_name, app_id, callback);
  HandlerRecordVector::iterator it = records_.begin();
  for (; it != records_.end(); it++) {
    if (**it == record)
      break;
  }
  if (it == records_.end())
    return;

  records_.erase(it);
  scoped_ptr<EventHandler> handler(new EventHandler(
        event_name, app_id, callback, GetWeakPtr()));
  RemoveEventHandler(handler.Pass());
}

void AppEventRegistrar::RemoveAll() {
  HandlerRecordVector::iterator it = records_.begin();
  for (; it != records_.end(); it++) {
    scoped_ptr<EventHandler> handler(new EventHandler(
          (*it)->event_name, (*it)->app_id, (*it)->callback, GetWeakPtr()));
    RemoveEventHandler(handler.Pass());
  }
  records_.clear();
}

void AppEventRegistrar::RemoveEventHandler(scoped_ptr<EventHandler> handler) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
          &ApplicationEventRouter::RemoveEventHandler,
          base::Unretained(router_), base::Passed(&handler)));
  } else {
    router_->RemoveEventHandler(handler.Pass());
  }
}

base::WeakPtr<AppEventRegistrar> AppEventRegistrar::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace application
}  // namespace xwalk
