#pragma once
#include <QObject>
#include <QSettings>
#include <QHash>
#include <QVariantList>
#include <QUrl>
class QLocalServer;
class QLocalSocket;
class DockBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool preview READ preview CONSTANT)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString dockEdge READ dockEdge WRITE setDockEdge NOTIFY dockEdgeChanged)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled NOTIFY animationsEnabledChanged)
    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)
    Q_PROPERTY(QVariantList pinnedApps READ pinnedApps NOTIFY appsChanged)
    Q_PROPERTY(QString platformStatus READ platformStatus NOTIFY platformStatusChanged)
public:
    explicit DockBackend(bool preview, QObject *parent = nullptr);
    bool preview() const { return m_preview; }
    QString theme() const { return m_theme; }
    QString dockEdge() const { return m_dockEdge; }
    bool animationsEnabled() const { return m_animationsEnabled; }
    QVariantList apps() const;
    QVariantList pinnedApps() const { return m_apps; }
    QString platformStatus() const { return m_platformStatus; }
    void setTheme(const QString &theme);
    void setDockEdge(const QString &edge);
    void setAnimationsEnabled(bool enabled);
    void setPlatformStatus(const QString &status);
    Q_INVOKABLE void launch(const QString &key, const QString &name, const QString &launchId = {});
    Q_INVOKABLE bool addPinnedApp(const QString &name, const QString &launchId, const QString &iconName);
    Q_INVOKABLE bool removePinnedApp(int index);
    Q_INVOKABLE void resetPinnedApps();
    Q_INVOKABLE bool pinApp(const QString &key);
    Q_INVOKABLE bool unpinApp(const QString &key);
    Q_INVOKABLE bool movePinnedApp(const QString &key, const QString &beforeKey);
    Q_INVOKABLE bool pinDesktopFile(const QUrl &url);
    void updateWindows(const QVariantList &windows);
    bool ownsEndpoint() const { return m_launchEvents != nullptr; }
signals:
    void themeChanged();
    void dockEdgeChanged();
    void animationsEnabledChanged();
    void appsChanged();
    void platformStatusChanged();
    void notice(const QString &message);
private:
    bool m_preview;
    QString m_theme;
    QString m_dockEdge;
    bool m_animationsEnabled = true;
    QVariantList m_apps;
    QVariantList m_runningApps;
    QVariantList m_windows;
    QHash<QString, QVariantMap> m_desktopApps;
    void reloadDesktopApps();
    QVariantMap resolveWindow(const QVariantMap &window) const;
    QString m_platformStatus;
    QSettings m_settings;
    QLocalServer *m_launchEvents = nullptr;
    QHash<QLocalSocket *, QByteArray> m_launchEventBuffers;
    void saveApps();
    void startLaunchEventServer();
    void readLaunchEvent(QLocalSocket *socket);
    void addRunningApp(const QString &name, const QString &launchId, const QString &icon);
};
