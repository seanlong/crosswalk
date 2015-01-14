// Copyright (c) 2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/renderer_host/pepper/xwalk_browser_pepper_host_factory.h"

#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/host/message_filter_host.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "xwalk/runtime/browser/renderer_host/pepper/pepper_isolated_file_system_message_filter.h"

using ppapi::host::MessageFilterHost;
using ppapi::host::ResourceHost;
using ppapi::host::ResourceMessageFilter;

namespace xwalk {

XWalkBrowserPepperHostFactory::XWalkBrowserPepperHostFactory(
    content::BrowserPpapiHost* host)
    : host_(host) {
}

XWalkBrowserPepperHostFactory::~XWalkBrowserPepperHostFactory() {
}

scoped_ptr<ResourceHost> XWalkBrowserPepperHostFactory::CreateResourceHost(
    ppapi::host::PpapiHost* host,
    const ppapi::proxy::ResourceMessageCallParams& params,
    PP_Instance instance,
    const IPC::Message& message) {
  DCHECK(host == host_->GetPpapiHost());
  // Make sure the plugin is giving us a valid instance for this resource.
  if (!host_->IsValidInstance(instance))
    return scoped_ptr<ResourceHost>();

  if (message.type() == PpapiHostMsg_IsolatedFileSystem_Create::ID) {
    PepperIsolatedFileSystemMessageFilter* isolated_fs_filter =
      PepperIsolatedFileSystemMessageFilter::Create(instance, host_);
    if (!isolated_fs_filter)
      return scoped_ptr<ResourceHost>();
    return scoped_ptr<ResourceHost>(
        new MessageFilterHost(host, instance, params.pp_resource(), isolated_fs_filter));
  }

  return scoped_ptr<ResourceHost>();
}

}  // namespace xwalk
