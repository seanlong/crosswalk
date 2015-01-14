#include "xwalk/runtime/browser/xwalk_special_storage_policy.h"

namespace xwalk {

  XWalkSpecialStoragePolicy::XWalkSpecialStoragePolicy() {
  }

  XWalkSpecialStoragePolicy::~XWalkSpecialStoragePolicy() {
  }

  bool XWalkSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
      return true;
  }

  bool XWalkSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
      return true;
  }

  bool XWalkSpecialStoragePolicy::IsStorageSessionOnly(const GURL& origin) {
      return false;
  }

  bool XWalkSpecialStoragePolicy::CanQueryDiskSize(const GURL& origin) {
      return true;
  }

  bool XWalkSpecialStoragePolicy::HasSessionOnlyOrigins() {
      return false;
  }

  bool XWalkSpecialStoragePolicy::IsFileHandler(const std::string& extension_id) {
      return true;
  }

  bool XWalkSpecialStoragePolicy::HasIsolatedStorage(const GURL& origin) {
      return true;
  }

}
