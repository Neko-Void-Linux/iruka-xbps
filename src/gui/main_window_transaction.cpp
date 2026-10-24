
#include "main_window_private.h"
#include "constants.h"
#include "i18n.h"
#include <string>
#include <vector>
#include <algorithm>

void MainWindow::addToTransaction(const std::string &pkgName, bool install) {
    if (install) {
        m_transactionInstall.push_back(pkgName);
        m_installMarks.insert(pkgName);
    } else {
        m_transactionRemove.push_back(pkgName);
        m_removeMarks.insert(pkgName);
    }

    GtkTreeIter iter;
    gtk_list_store_append(m_transStore, &iter);
    gtk_list_store_set(m_transStore, &iter,
        0, install ? "I" : "X",
        1, pkgName.c_str(),
        -1);

    setPackageStatus(pkgName, install ? "I" : "X");
}

void MainWindow::do_unmark() {
    GtkTreeIter   iter;
    GtkTreeModel *model = GTK_TREE_MODEL(m_listStore);
    if (!gtk_tree_selection_get_selected(m_selection, &model, &iter)) return;

    gpointer ptr = nullptr;
    gtk_tree_model_get(model, &iter, COL_PKG_PTR, &ptr, -1);
    if (!ptr) return;

    auto *pkg = static_cast<PackageRepository::PackageData*>(ptr);
    const std::string name = pkg->name;

    bool changed = false;
    auto eraseFrom = [&](std::vector<std::string> &v) {
        auto it = std::find(v.begin(), v.end(), name);
        if (it != v.end()) { v.erase(it); changed = true; }
    };
    eraseFrom(m_transactionInstall);
    eraseFrom(m_transactionRemove);
    if (m_installMarks.erase(name) > 0) changed = true;
    if (m_removeMarks.erase(name) > 0) changed = true;
    if (!changed) return;

    GtkTreeModel *tm = GTK_TREE_MODEL(m_transStore);
    GtkTreeIter   ti;
    gboolean valid = gtk_tree_model_get_iter_first(tm, &ti);
    while (valid) {
        gchar *rowName = nullptr;
        gtk_tree_model_get(tm, &ti, 1, &rowName, -1);
        if (rowName && name == rowName) {
            g_free(rowName);
            valid = gtk_list_store_remove(m_transStore, &ti);
        } else {
            g_free(rowName);
            valid = gtk_tree_model_iter_next(tm, &ti);
        }
    }

    std::string status = pkg->outdated() ? "!" : (pkg->installed() ? "@" : " ");
    setPackageStatus(name, status);
}

void MainWindow::setPackageStatus(const std::string &pkgName,
                                  const std::string &status) {
    GtkTreeModel *model = GTK_TREE_MODEL(m_listStore);
    GtkTreeIter   iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid) {
        gchar *name = nullptr;
        gtk_tree_model_get(model, &iter, COL_NAME, &name, -1);
        if (name && pkgName == name) {
            gtk_list_store_set(m_listStore, &iter, COL_STATUS, status.c_str(), -1);
            g_free(name);
            return;
        }
        g_free(name);
        valid = gtk_tree_model_iter_next(model, &iter);
    }
}

void MainWindow::commitTransaction() {
    if (m_transactionRunning) return;

    if (m_transactionInstall.empty() && m_transactionRemove.empty()) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(m_window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
            _("No pending transactions."));
        g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), nullptr);
        gtk_widget_show(dialog);
        return;
    }

    std::string msg = std::string(_("Execute transaction?")) + "\n\n";
    if (!m_transactionInstall.empty()) {
        msg += _("Install: ");
        for (auto &p : m_transactionInstall) msg += p + " ";
        msg += "\n";
    }
    if (!m_transactionRemove.empty()) {
        msg += _("Remove: ");
        for (auto &p : m_transactionRemove) msg += p + " ";
        msg += "\n";
    }

    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(m_window),
        GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
        "%s", msg.c_str());
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("OK"), GTK_RESPONSE_ACCEPT);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(dialog);
        return;
    }
    gtk_widget_destroy(dialog);

    m_commandQueue.clear();
    if (!m_transactionInstall.empty()) {
        std::vector<std::string> installArgs = { XBPS_INSTALL_BIN, "-y" };
        for (auto &p : m_transactionInstall) installArgs.push_back(p);
        m_commandQueue.push_back({ installArgs, _("Installing packages...") });
    }
    if (!m_transactionRemove.empty()) {
        std::vector<std::string> removeArgs = { XBPS_REMOVE_BIN, "-R", "-y" };
        for (auto &p : m_transactionRemove) removeArgs.push_back(p);
        m_commandQueue.push_back({ removeArgs, _("Removing packages...") });
    }

    clearTransaction();
    runCommandQueue();
}

void MainWindow::clearTransaction() {
    m_transactionInstall.clear();
    m_transactionRemove.clear();
    m_installMarks.clear();
    m_removeMarks.clear();
    gtk_list_store_clear(m_transStore);
    populateTable();
}
