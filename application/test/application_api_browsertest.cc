// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_util.h"
#include "xwalk/runtime/browser/runtime.h"
#include "xwalk/runtime/browser/runtime_registry.h"
#include "xwalk/runtime/common/xwalk_notification_types.h"
#include "xwalk/test/base/in_process_browser_test.h"

namespace {

bool WaitForRuntimeCountCallback(int* count) {
  --(*count);
  return *count == 0;
}

}  // namespace

using xwalk::RuntimeRegistry;

class ApplicationAPIBrowserTest: public InProcessBrowserTest {
 public:
  virtual void SetUp() OVERRIDE;
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  base::FilePath test_data_dir_;
};

void ApplicationAPIBrowserTest::SetUp() {
  PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir_);
  test_data_dir_ = test_data_dir_
    .Append(FILE_PATH_LITERAL("xwalk"))
    .Append(FILE_PATH_LITERAL("application"))
    .Append(FILE_PATH_LITERAL("test"))
    .Append(FILE_PATH_LITERAL("data"));
  InProcessBrowserTest::SetUp();
}

void ApplicationAPIBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  GURL url = net::FilePathToFileURL(test_data_dir_.Append(
        FILE_PATH_LITERAL("api")));
  command_line->AppendArg(url.spec());
}

IN_PROC_BROWSER_TEST_F(ApplicationAPIBrowserTest, APITest) {
  content::RunAllPendingInMessageLoop();
  const string16 pass = ASCIIToUTF16("Pass");
  const string16 fail = ASCIIToUTF16("Fail");

  const xwalk::RuntimeList& runtimes = RuntimeRegistry::Get()->runtimes();
  int count = 2 - runtimes.size() + 1;
  if (count > 1) {
    content::WindowedNotificationObserver(
        xwalk::NOTIFICATION_RUNTIME_OPENED,
        base::Bind(&WaitForRuntimeCountCallback, &count)).Wait();
  }
  ASSERT_EQ(2, RuntimeRegistry::Get()->runtimes().size());

  content::WebContents* web_contents = runtimes.at(1)->web_contents();
  if (web_contents->GetTitle() != pass && web_contents->GetTitle() != fail) {
    content::TitleWatcher title_watcher(web_contents, pass);
    title_watcher.AlsoWaitForTitle(fail);
    EXPECT_EQ(pass, title_watcher.WaitAndGetTitle());
    return;
  }
  ASSERT_EQ(pass, web_contents->GetTitle());
}
