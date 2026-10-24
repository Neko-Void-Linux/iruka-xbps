#include "package_repository.h"
#include "category.h"

PackageRepository::PackageRepository() {}

PackageRepository::~PackageRepository() {
  clear();
}

void PackageRepository::clear() {
  for (auto *p : m_listOfPackages) delete p;
  m_listOfPackages.clear();
  m_nameIndex.clear();
}

void PackageRepository::setData(const std::vector<PackageListData> &packages,
                                 const std::set<std::string> &unrequiredPackages) {
  clear();

  for (auto &pld : packages) {
    bool isRequired = unrequiredPackages.find(pld.name) == unrequiredPackages.end();
    auto *pkg = new PackageData(pld, isRequired);
    pkg->category = category::classify(pkg->name,
                                       pkg->description + " " + pkg->comment);
    m_listOfPackages.push_back(pkg);
    m_nameIndex[pkg->name] = pkg;
  }

  std::sort(m_listOfPackages.begin(), m_listOfPackages.end(),
    [](const PackageData *a, const PackageData *b) {
      return a->name < b->name;
    });
}

void PackageRepository::markOutdatedPackages(
    const std::map<std::string, OutdatedPackageInfo> &outdatedMap) {
  for (auto *pkg : m_listOfPackages) {
    auto it = outdatedMap.find(pkg->name);
    if (it != outdatedMap.end()) {
      pkg->outdatedVersion = it->second.newVersion;
      if (pkg->installed()) {
        pkg->status = PackageStatus::Outdated;
      }
    }
  }
}

PackageRepository::PackageData*
PackageRepository::getFirstPackageByName(const std::string &name) const {
  auto it = m_nameIndex.find(name);
  if (it != m_nameIndex.end()) return it->second;
  return nullptr;
}
