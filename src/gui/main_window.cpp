
#include "main_window_private.h"
#include "settings.h"
#include "repository_editor.h"
#include "privileged_daemon.h"

MainWindow::MainWindow()
    : m_popupMenu(nullptr), m_statusContextId(0), m_model(nullptr)
{
    m_model = new PackageModel(m_repo);
    buildUi();
    loadPackageList();
}

MainWindow::~MainWindow() {
    m_destroyed = true;
    PrivilegedDaemon::instance().stop();
    delete m_repoEditor;
    delete m_model;
}

void MainWindow::show() {
  gtk_widget_show_all(m_window);
  m_lastPanedPosition = static_cast<int>(Settings::instance().windowHeight() * 0.62);
  gtk_paned_set_position(GTK_PANED(m_mainPaned), m_lastPanedPosition);
  PrivilegedDaemon::instance().start();
}

void MainWindow::reloadUi() {
    if (m_rebuilding || m_transactionRunning || m_destroyed.load()) return;
    m_rebuilding = true;

    bool detailsVisible =
        gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(m_viewDetailsItem));

    if (m_searchDebounceId) {
        g_source_remove(m_searchDebounceId);
        m_searchDebounceId = 0;
    }
    if (m_sizeDebounceId) {
        g_source_remove(m_sizeDebounceId);
        m_sizeDebounceId = 0;
    }
    if (m_progressTimerId) {
        g_source_remove(m_progressTimerId);
        m_progressTimerId = 0;
    }

    ++m_infoGeneration;

    if (m_repoEditor) {
        m_repoEditor->close();
        delete m_repoEditor;
        m_repoEditor = nullptr;
    }

    g_signal_handlers_disconnect_by_func(m_window, (gpointer)G_CALLBACK(gtk_main_quit), nullptr);
    gtk_widget_destroy(m_window);
    if (m_progressDialog) gtk_widget_destroy(m_progressDialog);
    if (m_popupMenu) {
        gtk_widget_destroy(m_popupMenu);
        m_popupMenu = nullptr;
    }

    delete m_model;
    m_model = new PackageModel(m_repo);

    buildUi();
    show();

    if (!detailsVisible) {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(m_viewDetailsItem), FALSE);
    }

    GtkTreeIter iter;
    for (const auto &name : m_transactionInstall) {
        gtk_list_store_append(m_transStore, &iter);
        gtk_list_store_set(m_transStore, &iter, 0, "I", 1, name.c_str(), -1);
    }
    for (const auto &name : m_transactionRemove) {
        gtk_list_store_append(m_transStore, &iter);
        gtk_list_store_set(m_transStore, &iter, 0, "X", 1, name.c_str(), -1);
    }

    populateTable();
    refreshStatusBar();
    loadPackageList();

    m_rebuilding = false;
}
