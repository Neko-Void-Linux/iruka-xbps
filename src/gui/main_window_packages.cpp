
#include "main_window_private.h"
#include "xbps_command.h"
#include "settings.h"
#include "package.h"
#include "package_model.h"
#include "i18n.h"
#include <sstream>
#include <thread>
#include <mutex>
#include <map>
#include <vector>
#include <algorithm>
#include <cstdio>

void MainWindow::loadPackageList() {
    if (m_loading) return;
    m_loading = true;

    auto *result = new LoadResult();
    result->win = this;

    std::thread([result]() {
        auto localPkgs  = XBPSCommand::getLocalPackageList();
        auto remotePkgs = XBPSCommand::getRemotePackageList();

        std::map<std::string, PackageListData*> pkgMap;
        for (auto &pkg : remotePkgs) {
            auto *np = new PackageListData(std::move(pkg));
            auto it  = pkgMap.find(np->name);
            if (it != pkgMap.end()) {
                delete it->second;
                it->second = np;
            } else {
                pkgMap[np->name] = np;
            }
        }
        for (auto &pkg : localPkgs) {
            auto it = pkgMap.find(pkg.name);
            if (it != pkgMap.end()) {
                it->second->version     = pkg.version;
                it->second->status      = pkg.status;
                it->second->comment     = pkg.comment;
                it->second->description = pkg.description;
            } else {
                auto *np = new PackageListData(std::move(pkg));
                pkgMap[np->name] = np;
            }
        }

        for (auto &[name, pkg] : pkgMap) {
            result->merged.push_back(std::move(*pkg));
            delete pkg;
        }

        auto unrequired = XBPSCommand::getUnrequiredPackages();
        result->unreqSet.insert(unrequired.begin(), unrequired.end());

        result->outdated = XBPSCommand::getOutdatedPackages();

        g_idle_add(load_done_cb, result);
    }).detach();
}

void MainWindow::applyLoadedPackages(std::vector<PackageListData> &&packages,
                                     std::set<std::string>         &&unreqSet,
                                     std::map<std::string, OutdatedPackageInfo> &&outdated) {
    gtk_list_store_clear(m_listStore);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_repo.setData(packages, unreqSet);
        m_repo.markOutdatedPackages(outdated);
    }

    m_loading = false;
    applyFilters();
}

void MainWindow::populateTable() {
    gtk_list_store_clear(m_listStore);

    std::lock_guard<std::mutex> lock(m_mutex);
    int count = m_model->rowCount();
    for (int i = 0; i < count; i++) {
        auto *pkg = m_model->getData(i);
        if (!pkg) continue;

        GtkTreeIter iter;
        gtk_list_store_append(m_listStore, &iter);

        std::string status;
        if      (m_removeMarks.count(pkg->name) > 0) status = "X";
        else if (m_installMarks.count(pkg->name) > 0) status = "I";
        else if (pkg->outdated())  status = "!";
        else if (pkg->installed()) status = "@";
        else                       status = " ";

        gtk_list_store_set(m_listStore, &iter,
            COL_STATUS,      status.c_str(),
            COL_NAME,        pkg->name.c_str(),
            COL_VERSION,     pkg->version.c_str(),
            COL_DESCRIPTION, pkg->description.c_str(),
            COL_PKG_PTR,     (gpointer)pkg,
            -1);
    }
}

void MainWindow::refreshStatusBar() {
    std::lock_guard<std::mutex> lock(m_mutex);

    int total = static_cast<int>(m_repo.getPackageList().size());

    int outdated = 0;
    for (auto *p : m_repo.getPackageList()) {
        if (p->outdated()) outdated++;
    }

    char buf[64];
    std::ostringstream ss;
    std::snprintf(buf, sizeof(buf), _("Packages: %d"), total);
    ss << buf;
    std::snprintf(buf, sizeof(buf), _("Installed: %d"), m_model->getInstalledCount());
    ss << "    " << buf;
    std::snprintf(buf, sizeof(buf), _("Outdated: %d"), outdated);
    ss << "    " << buf;

    gtk_statusbar_remove_all(GTK_STATUSBAR(m_statusbar), m_statusContextId);
    gtk_statusbar_push(GTK_STATUSBAR(m_statusbar), m_statusContextId, ss.str().c_str());
}

void MainWindow::applyFilters() {
  const char *text  = gtk_entry_get_text(GTK_ENTRY(m_searchEntry));
  std::string query = text ? text : "";
  ViewOption view   = Settings::instance().viewOption();

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_model->applyFilter(view, "", m_selectedCategory);
    if (!query.empty()) {
      m_model->applyFilter(query);
    }
    m_model->sort(PackageModel::ColName, true);
  }
  populateTable();
  refreshStatusBar();
}

void MainWindow::onSearch() {
  applyFilters();
}

void MainWindow::applyCategory(int kind, const std::string &value) {
  switch (kind) {
    case 0: setView(ViewOption::All); break;
    case 1: setView(ViewOption::Installed); break;
    case 2: setView(ViewOption::NotInstalled); break;
    case 3: setView(ViewOption::Outdated); break;
    case 5:
      m_selectedCategory = value;
      Settings::instance().setViewOption(ViewOption::All);
      applyFilters();
      break;
    default:
      break;
  }
}

void MainWindow::onTableSelect() {
    if (m_rebuilding || m_destroyed.load()) return;

    gtk_text_buffer_set_text(m_sizeBuffer, "", -1);

    if (m_sizeDebounceId) {
        g_source_remove(m_sizeDebounceId);
        m_sizeDebounceId = 0;
    }
    m_sizeDebounceId = g_timeout_add(250, cb_size_debounce, this);
}

void MainWindow::onSelectionDebounced() {
    GtkTreeIter  iter;
    GtkTreeModel *model = GTK_TREE_MODEL(m_listStore);
    if (!gtk_tree_selection_get_selected(m_selection, &model, &iter)) return;

    gpointer ptr = nullptr;
    gtk_tree_model_get(model, &iter, COL_PKG_PTR, &ptr, -1);
    if (!ptr) return;

    auto *pkg = static_cast<PackageRepository::PackageData*>(ptr);
    int row = m_model->rowOfPackage(pkg);
    if (row >= 0) showPackageInfo(row);
}

void MainWindow::showPackageInfo(int row) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto *pkg = m_model->getData(row);
    if (!pkg) return;

    std::ostringstream ss;
    ss << _("Name:")         << " " << pkg->name        << "\n";
    ss << _("Version:")      << " " << pkg->version     << "\n";
    ss << _("Repository:")   << " " << pkg->repository  << "\n";
    ss << _("Description:")  << " " << pkg->description << "\n";

    bool        installed = pkg->installed();
    std::string pkgName   = pkg->name;

    auto *result      = new InfoResult();
    result->win        = this;
    result->generation = ++m_infoGeneration;
    result->infoText   = ss.str();

    gtk_text_buffer_set_text(m_infoBuffer, ss.str().c_str(), -1);
    gtk_text_buffer_set_text(m_filesBuffer, "", -1);

    std::thread([result, pkgName, installed]() {
        auto info = XBPSCommand::getPackageInfo(pkgName, installed);

        std::ostringstream ss;
        if (installed) {
            ss << "\n" << _("--- Package Details ---") << "\n";
            ss << _("Homepage:")     << "   " << info.url                  << "\n";
            ss << _("License:")      << "   " << info.license              << "\n";
            ss << _("Maintainer:")   << "   " << info.maintainer           << "\n";
            ss << _("Arch:")         << "   " << info.arch                 << "\n";
            ss << _("Build Date:")   << "   " << info.buildDate            << "\n";
            ss << _("Install Date:") << "   " << info.installDate          << "\n";
            ss << "\n" << _("Depends On:") << "   " << info.dependsOn      << "\n";
            ss << _("Opt Deps:")     << "   " << info.optDepends           << "\n";
            ss << _("Required By:")  << "   " << info.requiredBy           << "\n";
            ss << _("Conflicts:")    << "   " << info.conflictsWith        << "\n";
            ss << _("Replaces:")     << "   " << info.replaces             << "\n";
            ss << _("Options:")      << "   " << info.options              << "\n";
        } else {
            ss << "\n" << _("--- Remote Package Details ---") << "\n";
            ss << _("Homepage:")     << "   " << info.url                  << "\n";
            ss << _("License:")      << "   " << info.license              << "\n";
            ss << _("Maintainer:")   << "   " << info.maintainer           << "\n";
            ss << _("Arch:")         << "   " << info.arch                 << "\n";
            ss << "\n" << _("Depends On:") << "   " << info.dependsOn       << "\n";
            ss << _("Opt Deps:")     << "   " << info.optDepends           << "\n";
            ss << _("Conflicts:")    << "   " << info.conflictsWith        << "\n";
        }
        result->infoText += ss.str();

        std::ostringstream sizeSs;
        sizeSs << _("Name:")           << " " << pkgName                     << "\n";
        sizeSs << _("Download Size:")  << "   " << info.downloadSizeAsString << "\n";
        sizeSs << _("Installed Size:") << "   " << info.installedSizeAsString<< "\n";
        result->sizeText = sizeSs.str();

        auto files = XBPSCommand::getPackageFiles(pkgName, installed);
        std::ostringstream fss;
        for (auto &f : files) fss << f << "\n";
        result->filesText = fss.str();

        g_idle_add(info_done_cb, result);
    }).detach();
}

void MainWindow::applyPackageInfo(int generation, const std::string &infoText,
                                  const std::string &filesText,
                                  const std::string &sizeText) {
    if (generation != m_infoGeneration) return;
    gtk_text_buffer_set_text(m_infoBuffer,  infoText.c_str(),  -1);
    gtk_text_buffer_set_text(m_filesBuffer, filesText.c_str(), -1);
    gtk_text_buffer_set_text(m_sizeBuffer,  sizeText.c_str(),  -1);
}
