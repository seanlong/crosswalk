// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_APPLICATION_BROWSER_APPLICATION_EVENT_REGISTRAR_H_
#define XWALK_APPLICATION_BROWSER_APPLICATION_EVENT_REGISTRAR_H_

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "xwalk/application/browser/application_event_router.h"

namespace xwalk {
namespace application {

// Helper class for add/remove event handler to ApplicationEventRoute. It will
// remove all added handlers when the instance is destroyed.
// A weak pointer of this will be passed as the owner of EventHandler, so it can
// be safely destructed before the EventHandler finishes its task.
class AppEventRegistrar {
 public:
  explicit AppEventRegistrar(ApplicationEventRouter* router);
  ~AppEventRegistrar();

  void Add(const std::string& event_name,
           const std::string& app_id,
           const EventHandlerCallback& callback);

  void Remove(const std::string& event_name,
              const std::string& app_id,
              const EventHandlerCallback& callback);

  void RemoveAll();

 private:
  struct HandlerRecord {
    HandlerRecord(const std::string& event_name,
                  const std::string& app_id,
                  const EventHandlerCallback& callback);
    ~HandlerRecord();

    bool operator==(const HandlerRecord& other) const;

    std::string event_name;
    std::string app_id;
    EventHandlerCallback callback;
  };

  typedef std::vector<linked_ptr<HandlerRecord> > HandlerRecordVector;

  void RemoveEventHandler(scoped_ptr<EventHandler> handler);

  base::WeakPtr<AppEventRegistrar> GetWeakPtr();
 
  HandlerRecordVector records_;
  ApplicationEventRouter* router_;

  base::WeakPtrFactory<AppEventRegistrar> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppEventRegistrar);
};

}  // namespace application
}  // namespace xwalk

#endif  // XWALK_APPLICATION_BROWSER_APPLICATION_EVENT_REGISTRAR_H_
