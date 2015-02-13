// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/common/xwalk_switches.h"

namespace switches {

// Specifies the icon file for the app window.
const char kAppIcon[] = "app-icon";

// Disables the usage of Portable Native Client.
const char kDisablePnacl[] = "disable-pnacl";

// Enable all the experimental features in XWalk.
const char kExperimentalFeatures[] = "enable-xwalk-experimental-features";

// List the command lines feature flags.
const char kListFeaturesFlags[] = "list-features-flags";

const char kXWalkAllowExternalExtensionsForRemoteSources[] =
    "allow-external-extensions-for-remote-sources";

// Specifies the data path directory, which XWalk runtime will look for its
// state, e.g. cache, localStorage etc.
const char kXWalkDataPath[] = "data-path";

#if defined(OS_ANDROID)
// Specifies the separated folder to save user data on Android.
const char kXWalkProfileName[] = "profile-name";
#endif

#if !defined(DISABLE_NACL)
// Use the PPAPI (Pepper) Flash found at the given path.
const char kPpapiFlashPath[] = "ppapi-flash-path";

// Report the given version for the PPAPI (Pepper) Flash. The version should be
// numbers separated by '.'s (e.g., "12.3.456.78"). If not specified, it
// defaults to "10.2.999.999".
const char kPpapiFlashVersion[] = "ppapi-flash-version";
#endif

}  // namespace switches
