
#ifndef XWALK_RUNTIME_BROWSER_XWALK_SPECIAL_STORAGE_POLICY_H_
#define XWALK_RUNTIME_BROWSER_XWALK_SPECIAL_STORAGE_POLICY_H_

#include "storage/browser/quota/special_storage_policy.h"
namespace xwalk {

  
class XWalkSpecialStoragePolicy : public storage::SpecialStoragePolicy {
  public:
    XWalkSpecialStoragePolicy();

    // storage::SpecialStoragePolicy implementation.
    bool IsStorageProtected(const GURL& origin) override;
    bool IsStorageUnlimited(const GURL& origin) override;
    bool IsStorageSessionOnly(const GURL& origin) override;
    bool CanQueryDiskSize(const GURL& origin) override;
    bool IsFileHandler(const std::string& extension_id) override;
    bool HasIsolatedStorage(const GURL& origin) override;
    bool HasSessionOnlyOrigins() override;

  protected:
    ~XWalkSpecialStoragePolicy() override;
};

}

#endif
