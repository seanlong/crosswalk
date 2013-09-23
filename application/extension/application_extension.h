// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_APPLICATION_EXTENSION_APPLICATION_EXTENSION_H_
#define XWALK_APPLICATION_EXTENSION_APPLICATION_EXTENSION_H_

#include <string>
#include "xwalk/extensions/browser/xwalk_extension_internal.h"

namespace xwalk {

class RuntimeContext;

class ApplicationExtension : public extensions::XWalkInternalExtension {
 public:
  explicit ApplicationExtension(RuntimeContext* runtime_context);

  virtual const char* GetJavaScriptAPI() OVERRIDE;

  virtual extensions::XWalkExtensionInstance* CreateInstance() OVERRIDE;

 private:
  RuntimeContext* runtime_context_;
};

class ApplicationExtensionInstance
: public extensions::XWalkInternalExtensionInstance {
 public:
  explicit ApplicationExtensionInstance(RuntimeContext* runtime_context);

 private:
  void OnGetMainDocumentID(const std::string& function_name,
                           const std::string& callback_id,
                           base::ListValue* args);

  // Get main document routing ID from ApplicationProcessManager on UI thread.
  void GetMainDocumentID();
  // Post message back to renderer on extension thread.
  void PostMainDocumentID(const std::string& callback_id);

  // Use RuntimeContext to retrieve ApplicationProcessManager.
  RuntimeContext* runtime_context_;
  int main_routing_id_;
};

}  // namespace xwalk

#endif  // XWALK_APPLICATION_EXTENSION_APPLICATION_EXTENSION_H_
