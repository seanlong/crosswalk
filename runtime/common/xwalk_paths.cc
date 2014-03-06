// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/common/xwalk_paths.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "xwalk/runtime/browser/xwalk_runner.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#endif

namespace xwalk {

namespace {

#if defined(OS_LINUX)
base::FilePath GetConfigPath() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  return base::nix::GetXDGDirectory(
      env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir);
}
#endif

bool GetXWalkDataPath(base::FilePath* path) {
  base::FilePath::StringType xwalk_suffix;
  if (XWalkRunner::GetInstance()->is_running_as_service())
    xwalk_suffix = FILE_PATH_LITERAL("xwalk-service");
  else
    xwalk_suffix = FILE_PATH_LITERAL("xwalk");
  base::FilePath cur;

#if defined(OS_WIN)
  CHECK(PathService::Get(base::DIR_LOCAL_APP_DATA, &cur));
  cur = cur.Append(xwalk_suffix);

#elif defined(OS_TIZEN_MOBILE)
  if (XWalkRunner::GetInstance()->is_running_as_service())
    cur = GetConfigPath().Append(xwalk_suffix);
  else
    cur = base::FilePath("/opt/usr/apps");

#elif defined(OS_LINUX)
  cur = GetConfigPath().Append(xwalk_suffix);

#elif defined(OS_MACOSX)
  CHECK(PathService::Get(base::DIR_APP_DATA, &cur));
  cur = cur.Append(xwalk_suffix);

#else
  NOTIMPLEMENTED() << "Unsupported OS platform.";
  return false;
#endif

  *path = cur;
  return true;
}

}  // namespace

bool PathProvider(int key, base::FilePath* path) {
  base::FilePath cur;
  switch (key) {
    case xwalk::DIR_DATA_PATH:
      return GetXWalkDataPath(path);
    case xwalk::DIR_TEST_DATA:
      if (!PathService::Get(base::DIR_SOURCE_ROOT, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("xwalk"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      cur = cur.Append(FILE_PATH_LITERAL("data"));
      break;
    default:
      return false;
  }
  *path = cur;
  return true;
}

void RegisterPathProvider() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace xwalk

