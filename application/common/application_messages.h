// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>

#include "base/memory/shared_memory.h"
#include "base/values.h"
#include "ipc/ipc_message_macros.h"
#include "xwalk/application/common/application.h"
#include "xwalk/application/common/manifest.h"

// Use chrome extension message slot.
const int ApplicationMsgStart = ExtensionMsgStart;

#define IPC_MESSAGE_START ApplicationMsgStart

// Singly-included section for custom IPC traits.
#ifndef XWALK_APPLICATION_COMMON_APPLICATION_MESSAGES_H_
#define XWALK_APPLICATION_COMMON_APPLICATION_MESSAGES_H_

using xwalk::application::Application;
using xwalk::application::Manifest;

struct ApplicationMsg_Launched_Params {
  ApplicationMsg_Launched_Params();
  ~ApplicationMsg_Launched_Params();
  explicit ApplicationMsg_Launched_Params(const Application* application);

  // Create new application from the data in this object.
  scoped_refptr<Application> ConvertToApplication(std::string* error) const;

  linked_ptr<base::DictionaryValue> manifest;
  Manifest::SourceType source_type;
  base::FilePath path;
  std::string id;
};

namespace IPC {

template <>
struct ParamTraits<ApplicationMsg_Launched_Params> {
  typedef ApplicationMsg_Launched_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // XWALK_APPLICATION_COMMON_APPLICATION_MESSAGES_H_

// Notifies the renderer that the application was launched.
IPC_MESSAGE_CONTROL1(ApplicationMsg_Launched,  // NOLINT(*)
                     ApplicationMsg_Launched_Params)

IPC_MESSAGE_CONTROL0(ApplicationMsg_Show)

IPC_MESSAGE_CONTROL0(ApplicationMsg_ShowAck)

IPC_MESSAGE_CONTROL0(ApplicationMsg_Hide)
