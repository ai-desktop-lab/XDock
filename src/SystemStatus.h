#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include <functional>
class QProcess;
class QSocketNotifier;

// Read-only host telemetry; user audio/tool actions use fixed executable arguments.
class SystemStatus final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap clock READ clock NOTIFY clockChanged)
    Q_PROPERTY(QVariantMap network READ network NOTIFY networkChanged)
    Q_PROPERTY(QVariantMap audio READ audio NOTIFY audioChanged)
    Q_PROPERTY(QVariantMap system READ system NOTIFY systemChanged)
    Q_PROPERTY(QVariantMap tools READ tools CONSTANT)
    Q_PROPERTY(QStringList weekdays READ weekdays CONSTANT)
    Q_PROPERTY(bool panelOpen READ panelOpen WRITE setPanelOpen NOTIFY panelOpenChanged)
    Q_PROPERTY(bool preview READ preview CONSTANT)
public:
    explicit SystemStatus(bool preview, QObject *parent=nullptr);
    ~SystemStatus() override;
    QVariantMap clock() const { return m_clock; }
    QVariantMap network() const { return m_network; }
    QVariantMap audio() const { return m_audio; }
    QVariantMap system() const { return m_system; }
    QVariantMap tools() const { return m_tools; }
    QStringList weekdays() const;
    bool panelOpen() const { return m_panelOpen; }
    bool preview() const { return m_preview; }
    void setPanelOpen(bool open);
    Q_INVOKABLE QVariantList calendarDays(int year, int month) const;
    Q_INVOKABLE QString calendarTitle(int year, int month) const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setVolume(int percent);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void openTool(const QString &key);
signals:
    void clockChanged();
    void networkChanged();
    void audioChanged();
    void systemChanged();
    void panelOpenChanged();
    void failure(const QString &message);
private slots:
    void networkPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);
private:
    bool m_preview, m_panelOpen=false, m_statsBusy=false, m_audioBusy=false, m_audioDirty=false;
    bool m_networkBusy=false, m_networkDirty=false, m_audioControlBusy=false;
    int m_pendingVolume=-1, m_routeFd=-1;
    quint64 m_cpuTotal=0, m_cpuIdle=0;
    quint64 m_networkGeneration=0;
    QString m_wpctl, m_pactl, m_audioProgram, m_primaryPath;
    QVariantMap m_clock, m_network, m_audio, m_system, m_tools;
    QTimer m_clockTimer, m_sampleTimer, m_networkDebounce, m_audioDebounce, m_volumeDebounce;
    QProcess *m_audioEvents=nullptr;
    QSocketNotifier *m_routeEvents=nullptr;
    void updateClock();
    void refreshNetwork();
    void refreshAudio();
    void sampleSystem();
    void startAudioEvents();
    void flushVolume();
    void run(const QString &program, const QStringList &args, std::function<void(bool,const QString &)> finished);
};
