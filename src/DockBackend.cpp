#include "DockBackend.h"
#include "platform/PlatformAdapter.h"
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocalServer>
#include <QLocalSocket>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent>
#include <utility>
#include <QDirIterator>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QStandardPaths>
#include <QLocale>
#include <QTimer>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QSet>

namespace {
QVariantMap dockApp(const QString &key, const QString &name, const QString &icon,
                    const QString &launchId = {}) {
    return {{"key", key}, {"name", name}, {"icon", icon}, {"launchId", launchId}};
}

constexpr auto launchEventServerName = "ai-workspace-lab.xdock.launch-events";

QString endpointName() {
    QString session = qEnvironmentVariable("DISPLAY", qEnvironmentVariable("WAYLAND_DISPLAY", "default"));
    if (!qEnvironmentVariable("DISPLAY").isEmpty()) session = session.section('.', 0, 0);
    session.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    return QString::fromLatin1(launchEventServerName) + "." + session;
}
QVariantList defaultApps() { return {}; }
QVariantList readApps(const QSettings &settings) {
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(settings.value("applications/pinned").toByteArray(), &error);
    QVariantList pins;
    if (error.error != QJsonParseError::NoError || !document.isArray()) return pins;
    for (auto entry : document.toVariant().toList()) {
        auto app = entry.toMap();
#ifdef Q_OS_LINUX
        auto id = app.value("launchId").toString();
        if (!id.isEmpty() && !QFileInfo(id).isAbsolute()) {
            auto path = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, id.endsWith(".desktop") ? id : id + ".desktop");
            if (!path.isEmpty()) app["launchId"] = path;
        }
#endif
        // Migrate old placeholder shortcuts; preserve explicitly configured apps.
        if (!app.value("launchId").toString().isEmpty() && !app.value("name").toString().isEmpty()) pins.append(app);
    }
    return pins;
}
}

DockBackend::DockBackend(bool preview, QObject *parent)
    : QObject(parent), m_preview(preview) {
    m_theme = preview ? "classic" : m_settings.value("appearance/theme", "classic").toString();
    m_dockEdge = preview ? "bottom" : m_settings.value("placement/edge", "bottom").toString();
    m_animationsEnabled = preview || m_settings.value("appearance/animationsEnabled", false).toBool();
    if (m_dockEdge != "bottom" && m_dockEdge != "top") m_dockEdge = "bottom";
    m_apps = preview ? defaultApps() : readApps(m_settings);
    if (!preview) {
        reloadDesktopApps();
        auto *watcher = new QFileSystemWatcher(this);
        auto *reload = new QTimer(this);
        reload->setSingleShot(true); reload->setInterval(250);
        for (const auto &root : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation))
            if (QFileInfo::exists(root)) watcher->addPath(root);
        connect(watcher, &QFileSystemWatcher::directoryChanged, reload, [reload] { reload->start(); });
        connect(reload, &QTimer::timeout, this, [this] { reloadDesktopApps(); updateWindows(m_windows); });
        startLaunchEventServer();
    }
}
QVariantList DockBackend::apps() const {
    QVariantList combined;
    auto launcher = dockApp("launcher", "应用菜单", "view-app-grid"); launcher["utility"] = true;
    auto desktop = dockApp("desktop", "显示桌面", "user-desktop"); desktop["utility"] = true;
    combined << launcher << desktop;
    for (auto entry : m_apps) {
        auto pin = entry.toMap(); pin["pinned"] = true;
        for (const auto &running : m_runningApps) {
            auto app = running.toMap();
            if (app.value("launchId") == pin.value("launchId") || app.value("key") == pin.value("key")) {
                pin["windows"] = app.value("windows"); pin["active"] = app.value("active"); break;
            }
        }
        combined.append(pin);
    }
    for (const auto &running : m_runningApps) {
        auto app = running.toMap(); bool pinned = false;
        for (const auto &entry : m_apps) pinned |= entry.toMap().value("launchId") == app.value("launchId") && !app.value("launchId").toString().isEmpty();
        if (!pinned) combined.append(app);
    }
    return combined;
}
void DockBackend::reloadDesktopApps() {
#ifdef Q_OS_LINUX
    m_desktopApps.clear();
    QSet<QString> seen;
    for (const auto &root : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)) {
        QDirIterator files(root, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            QFileInfo file(files.next());
            QString desktopId = QDir(root).relativeFilePath(file.filePath()); desktopId.replace('/', '-');
            if (seen.contains(desktopId)) continue;
            seen.insert(desktopId);
            QSettings entry(file.filePath(), QSettings::IniFormat);
            if (entry.value("Desktop Entry/Hidden").toBool() || entry.value("Desktop Entry/Type").toString() != "Application") continue;
            QString name = entry.value("Desktop Entry/Name[" + QLocale().name() + "]", entry.value("Desktop Entry/Name")).toString();
            auto app = dockApp("desktop:" + desktopId, name, entry.value("Desktop Entry/Icon", "application-x-executable").toString(), file.absoluteFilePath());
            QString executable = entry.value("Desktop Entry/Exec").toString().section(' ', 0, 0).remove('"');
            QStringList ids{file.completeBaseName(), entry.value("Desktop Entry/StartupWMClass").toString(), QFileInfo(executable).fileName()};
            for (const auto &id : ids) if (!id.isEmpty() && !m_desktopApps.contains(id.toLower())) m_desktopApps.insert(id.toLower(), app);
        }
    }
#endif
}
QVariantMap DockBackend::resolveWindow(const QVariantMap &window) const {
    for (const auto &id : QStringList{window.value("appId").toString(), window.value("wmClass").toString(), window.value("instance").toString(), window.value("executable").toString()}) {
        auto it = m_desktopApps.constFind(id.toLower());
        if (it != m_desktopApps.cend()) return *it;
    }
    QString identity = window.value("wmClass").toString();
    if (identity.isEmpty()) identity = window.value("instance").toString();
    if (identity.isEmpty()) identity = window.value("title").toString();
    auto app = dockApp("window:" + identity, identity, "window:" + window.value("id").toString());
    return app;
}
void DockBackend::updateWindows(const QVariantList &windows) {
    m_windows = windows;
    QVariantList running;
    QHash<QString, int> groups;
    for (const auto &entry : windows) {
        auto window = entry.toMap(); auto app = resolveWindow(window); auto key = app.value("key").toString();
        if (!groups.contains(key)) {
            groups[key] = running.size(); app["transient"] = true; app["active"] = false; app["windows"] = QVariantList{}; running.append(app);
        }
        int i = groups[key]; auto group = running[i].toMap(); auto list = group.value("windows").toList(); list.append(window);
        group["windows"] = list; group["active"] = group.value("active").toBool() || window.value("active").toBool(); running[i] = group;
    }
    if (running != m_runningApps) { m_runningApps = running; emit appsChanged(); }
}
bool DockBackend::pinApp(const QString &key) {
    for (auto entry : apps()) {
        auto app = entry.toMap();
        if (app.value("key").toString() == key) return addPinnedApp(app.value("name").toString(), app.value("launchId").toString(), app.value("icon").toString());
    }
    return false;
}
bool DockBackend::unpinApp(const QString &key) {
    for (int i = 0; i < m_apps.size(); ++i) if (m_apps[i].toMap().value("key").toString() == key) return removePinnedApp(i);
    return false;
}
bool DockBackend::movePinnedApp(const QString &key, const QString &beforeKey) {
    int from = -1, to = m_apps.size();
    for (int i = 0; i < m_apps.size(); ++i) {
        if (m_apps[i].toMap().value("key").toString() == key) from = i;
        if (m_apps[i].toMap().value("key").toString() == beforeKey) to = i;
    }
    if (from < 0 || from == to) return false;
    auto entry = m_apps.takeAt(from); if (from < to) --to; m_apps.insert(to, entry);
    saveApps(); emit appsChanged(); return true;
}
bool DockBackend::pinDesktopFile(const QUrl &url) {
#ifdef Q_OS_LINUX
    QFileInfo file(url.isLocalFile() ? url.toLocalFile() : url.toString());
    if (!file.isFile() || file.suffix() != "desktop") return false;
    QSettings entry(file.absoluteFilePath(), QSettings::IniFormat);
    if (entry.value("Desktop Entry/Type").toString() != "Application" || entry.value("Desktop Entry/Hidden").toBool()) return false;
    return addPinnedApp(entry.value("Desktop Entry/Name").toString(), file.absoluteFilePath(), entry.value("Desktop Entry/Icon").toString());
#else
    Q_UNUSED(url); return false;
#endif
}
void DockBackend::setTheme(const QString &theme) {
    if ((theme != "classic" && theme != "system") || m_theme == theme) return;
    m_theme = theme;
    if (!m_preview) {
        m_settings.setValue("appearance/theme", theme);
        m_settings.sync();
    }
    emit themeChanged();
}
void DockBackend::setPlatformStatus(const QString &status) {
    m_platformStatus = status;
    emit platformStatusChanged();
}
void DockBackend::setDockEdge(const QString &edge) {
    if (edge != "bottom" && edge != "top" || m_dockEdge == edge) return;
    m_dockEdge = edge;
    if (!m_preview) {
        m_settings.setValue("placement/edge", edge);
        m_settings.sync();
    }
    emit dockEdgeChanged();
}
void DockBackend::setAnimationsEnabled(bool enabled) {
    if (m_animationsEnabled == enabled) return;
    m_animationsEnabled = enabled;
    if (!m_preview) {
        m_settings.setValue("appearance/animationsEnabled", enabled);
        m_settings.sync();
    }
    emit animationsEnabledChanged();
}
void DockBackend::saveApps() {
    if (!m_preview) {
        m_settings.setValue("applications/pinned", QJsonDocument::fromVariant(m_apps).toJson(QJsonDocument::Compact));
        m_settings.sync();
    }
}
bool DockBackend::addPinnedApp(const QString &name, const QString &launchId, const QString &iconName) {
    const auto trimmedName = name.trimmed();
    auto trimmedLaunchId = launchId.trimmed();
#ifdef Q_OS_LINUX
    if (!QFileInfo(trimmedLaunchId).isAbsolute()) {
        QString id = trimmedLaunchId.endsWith(".desktop") ? trimmedLaunchId : trimmedLaunchId + ".desktop";
        auto path = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, id);
        if (path.isEmpty()) { emit notice("请选择已安装的 .desktop 应用。"); return false; }
        trimmedLaunchId = path;
    }
    if (!QFileInfo(trimmedLaunchId).isFile() || !trimmedLaunchId.endsWith(".desktop")) return false;
    QSettings desktop(trimmedLaunchId, QSettings::IniFormat);
    if (desktop.value("Desktop Entry/Type").toString() != "Application" || desktop.value("Desktop Entry/Hidden").toBool()) return false;
#endif
    if (trimmedName.isEmpty() || trimmedLaunchId.isEmpty()) return false;
    for (const auto &entry : m_apps) {
        if (entry.toMap().value("launchId").toString() == trimmedLaunchId) return false;
    }
    const auto key = QStringLiteral("custom-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto icon = iconName.trimmed();
#ifdef Q_OS_LINUX
    if (icon.isEmpty()) icon = desktop.value("Desktop Entry/Icon").toString();
#endif
    m_apps.append(dockApp(key, trimmedName,
                          icon.isEmpty() ? QStringLiteral("application-x-executable") : icon,
                          trimmedLaunchId));
    saveApps();
    emit appsChanged();
    return true;
}
bool DockBackend::removePinnedApp(int index) {
    if (index < 0 || index >= m_apps.size()) return false;
    m_apps.removeAt(index);
    saveApps();
    emit appsChanged();
    return true;
}
void DockBackend::resetPinnedApps() {
    m_apps = defaultApps();
    saveApps();
    emit appsChanged();
}
void DockBackend::startLaunchEventServer() {
    m_launchEvents = new QLocalServer(this);
    m_launchEvents->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_launchEvents->listen(endpointName())) {
        // Another XDock may already own the endpoint. Probe it before removing
        // a stale Unix-domain socket so a second launch cannot disrupt it.
        QLocalSocket probe;
        probe.connectToServer(endpointName());
        if (!probe.waitForConnected(100)) {
            QLocalServer::removeServer(endpointName());
            m_launchEvents->listen(endpointName());
        }
        if (!m_launchEvents->isListening()) {
            m_launchEvents->deleteLater();
            m_launchEvents = nullptr;
            return;
        }
    }
    connect(m_launchEvents, &QLocalServer::newConnection, this, [this] {
        while (m_launchEvents->hasPendingConnections()) {
            auto *socket = m_launchEvents->nextPendingConnection();
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] { readLaunchEvent(socket); });
            connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
                readLaunchEvent(socket);
                m_launchEventBuffers.remove(socket);
                socket->deleteLater();
            });
        }
    });
}
void DockBackend::readLaunchEvent(QLocalSocket *socket) {
    auto &buffer = m_launchEventBuffers[socket];
    buffer.append(socket->readAll());
    if (buffer.size() > 16384) { socket->abort(); return; }
    while (true) {
        const auto newline = buffer.indexOf('\n');
        if (newline < 0) break;
        const auto line = buffer.left(newline);
        buffer.remove(0, newline + 1);
        QJsonParseError error;
        const auto event = QJsonDocument::fromJson(line, &error).object();
        if (error.error != QJsonParseError::NoError || !QStringList{"launched", "pin"}.contains(event.value("type").toString())) continue;
        const auto name = event.value("name").toString().trimmed();
        const auto launchId = event.value("launchId").toString().trimmed();
        if (name.isEmpty() || launchId.isEmpty()) continue;
        if (event.value("type").toString() == "pin") {
            const bool ok = pinDesktopFile(QUrl::fromLocalFile(launchId));
            socket->write(ok ? "ok\n" : "error\n"); socket->flush(); continue;
        }
        // X11 window events are authoritative: no ghost icons after process exit.
        if (QGuiApplication::platformName() != "xcb") addRunningApp(name, launchId, event.value("icon").toString());
        socket->write("ok\n");
        socket->flush();
    }
}
void DockBackend::addRunningApp(const QString &name, const QString &launchId, const QString &icon) {
    for (const auto &entry : std::as_const(m_apps)) {
        if (entry.toMap().value("launchId").toString() == launchId) return;
    }
    for (const auto &entry : std::as_const(m_runningApps)) {
        if (entry.toMap().value("launchId").toString() == launchId) return;
    }
    auto app = dockApp(QStringLiteral("running-") + QUuid::createUuid().toString(QUuid::WithoutBraces),
                       name, icon.isEmpty() ? QStringLiteral("application-x-executable") : icon, launchId);
    app.insert("transient", true);
    m_runningApps.append(app);
    emit appsChanged();
}
void DockBackend::launch(const QString &key, const QString &name, const QString &launchId) {
    if (m_preview) {
        emit notice(QStringLiteral("预览 · %1\n正式运行时将打开已安装的系统应用。").arg(name));
        return;
    }
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher] {
        const auto error = watcher->result();
        if (!error.isEmpty()) emit notice(error);
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([key, launchId] { return PlatformAdapter::launch(key, launchId); }));
}
