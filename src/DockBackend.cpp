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

namespace {
QVariantMap dockApp(const QString &key, const QString &name, const QString &icon,
                    const QString &launchId = {}) {
    return {{"key", key}, {"name", name}, {"icon", icon}, {"launchId", launchId}};
}

constexpr auto launchEventServerName = "ai-workspace-lab.xdock.launch-events";

QVariantList defaultApps() {
    return {
        dockApp("launcher", "XLaunch", "view-app-grid"),
        dockApp("desktop", "显示桌面", "user-desktop"),
        dockApp("files", "文件管理器", "system-file-manager"),
        dockApp("browser", "浏览器", "firefox"),
        dockApp("music", "音乐", "multimedia-audio-player"),
        dockApp("video", "视频", "multimedia-video-player"),
        dockApp("store", "应用商店", "system-software-install"),
        dockApp("games", "游戏中心", "applications-games"),
        dockApp("screenshot", "截图", "applets-screenshooter"),
        dockApp("terminal", "终端", "utilities-terminal"),
        dockApp("settings", "系统设置", "preferences-system")
    };
}

QVariantList readApps(const QSettings &settings) {
    const auto encoded = settings.value("applications/pinned").toByteArray();
    if (encoded.isEmpty()) return defaultApps();
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(encoded, &error);
    const auto apps = document.toVariant().toList();
    if (error.error != QJsonParseError::NoError || apps.isEmpty()) return defaultApps();
    for (const auto &entry : apps) {
        const auto app = entry.toMap();
        if (app.value("key").toString().isEmpty() || app.value("name").toString().isEmpty())
            return defaultApps();
    }
    return apps;
}
}

DockBackend::DockBackend(bool preview, QObject *parent)
    : QObject(parent), m_preview(preview) {
    m_theme = preview ? "classic" : m_settings.value("appearance/theme", "classic").toString();
    m_dockEdge = preview ? "bottom" : m_settings.value("placement/edge", "bottom").toString();
    m_animationsEnabled = preview || m_settings.value("appearance/animationsEnabled", false).toBool();
    if (m_dockEdge != "bottom" && m_dockEdge != "top") m_dockEdge = "bottom";
    m_apps = preview ? defaultApps() : readApps(m_settings);
    if (!preview) startLaunchEventServer();
}
QVariantList DockBackend::apps() const {
    auto combined = m_apps;
    combined.append(m_runningApps);
    return combined;
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
    const auto trimmedLaunchId = launchId.trimmed();
    if (trimmedName.isEmpty() || trimmedLaunchId.isEmpty()) return false;
    for (const auto &entry : m_apps) {
        if (entry.toMap().value("launchId").toString() == trimmedLaunchId) return false;
    }
    const auto key = QStringLiteral("custom-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_apps.append(dockApp(key, trimmedName,
                          iconName.trimmed().isEmpty() ? QStringLiteral("application-x-executable") : iconName.trimmed(),
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
    if (!m_launchEvents->listen(QString::fromLatin1(launchEventServerName))) {
        // Another XDock may already own the endpoint. Probe it before removing
        // a stale Unix-domain socket so a second launch cannot disrupt it.
        QLocalSocket probe;
        probe.connectToServer(QString::fromLatin1(launchEventServerName));
        if (!probe.waitForConnected(100)) {
            QLocalServer::removeServer(QString::fromLatin1(launchEventServerName));
            m_launchEvents->listen(QString::fromLatin1(launchEventServerName));
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
    while (true) {
        const auto newline = buffer.indexOf('\n');
        if (newline < 0) break;
        const auto line = buffer.left(newline);
        buffer.remove(0, newline + 1);
        QJsonParseError error;
        const auto event = QJsonDocument::fromJson(line, &error).object();
        if (error.error != QJsonParseError::NoError || event.value("type").toString() != "launched") continue;
        const auto name = event.value("name").toString().trimmed();
        const auto launchId = event.value("launchId").toString().trimmed();
        if (name.isEmpty() || launchId.isEmpty()) continue;
        addRunningApp(name, launchId, event.value("icon").toString());
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
