#include "SystemStatus.h"
#include <QDateTime>
#include <QTimeZone>
#include <QLocale>
#include <QFile>
#include <QDir>
#include <QNetworkInterface>
#include <QStorageInfo>
#include <QSysInfo>
#include <QThread>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QSocketNotifier>
#include <memory>
#include <climits>
#ifdef SESSION_ACTIONS_DBUS
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#endif
#ifdef Q_OS_LINUX
#include <sys/socket.h>
#include <linux/rtnetlink.h>
#include <unistd.h>
#endif
namespace {
QString read(const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed() : QString(); }
QString bytes(qint64 n) { return n<0 ? QStringLiteral("不可用") : QLocale().formattedDataSize(n,1,QLocale::DataSizeIecFormat); }
QString routeInterface() {
#ifdef Q_OS_LINUX
    QString best; int metric=INT_MAX;
    for (const auto &line:read("/proc/net/route").split('\n')) {
        const auto fields=line.simplified().split(' ');
        if(fields.size()>7 && fields[1]=="00000000" && (fields[3].toUInt(nullptr,16)&1) && fields[6].toInt()<metric) {best=fields[0];metric=fields[6].toInt();}
    }
    return best;
#else
    return {};
#endif
}
QVariantMap localNetwork() {
    QVariantList links; const auto route=routeInterface(); QString summary;
    for(const auto &iface:QNetworkInterface::allInterfaces()) {
        if(iface.flags().testFlag(QNetworkInterface::IsLoopBack) || !iface.flags().testFlag(QNetworkInterface::IsUp) || !iface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
        QStringList addresses;
        for(const auto &entry:iface.addressEntries()) if(!entry.ip().isNull() && !entry.ip().isLoopback() && !entry.ip().isLinkLocal()) addresses.append(entry.ip().toString());
        if(addresses.isEmpty()) continue;
        links.append(QVariantMap{{"name",iface.name()},{"addresses",addresses.join(" · ")},{"defaultRoute",iface.name()==route}});
        if(summary.isEmpty()||iface.name()==route) summary=iface.humanReadableName();
    }
    return {{"connected",!links.isEmpty()},{"name",summary.isEmpty()?"未连接":summary},{"state",links.isEmpty()?"网络未连接":"有网络接口 · 互联网状态未检测"},{"icon",links.isEmpty()?"network-offline":"network-wired"},{"interfaces",links},{"route",route.isEmpty()?"无 IPv4 默认路由":route},{"source","系统接口"}};
}
struct Sample { QVariantMap values; quint64 total=0,idle=0; };
Sample hostSample(quint64 previousTotal, quint64 previousIdle) {
    Sample s;
    s.values={{"host",QSysInfo::machineHostName()},{"os",QSysInfo::prettyProductName()},{"cpuPercent",-1},{"memoryPercent",-1},{"batteryPresent",false},{"available",false}};
#ifdef Q_OS_LINUX
    const auto cpu=read("/proc/stat").section('\n',0,0).simplified().split(' ');
    if(cpu.size()>=9 && cpu[0]=="cpu") {
        for(int i=1;i<=8;++i)s.total+=cpu[i].toULongLong();
        s.idle=cpu[4].toULongLong()+cpu[5].toULongLong();
        if(previousTotal && s.total>previousTotal && s.idle>=previousIdle) s.values["cpuPercent"]=qBound(0,qRound(100.0*(1-double(s.idle-previousIdle)/double(s.total-previousTotal))),100);
        s.values["available"]=true;
    }
    s.values["cores"]=QThread::idealThreadCount();
    QMap<QString,qint64> memory;
    for(const auto &line:read("/proc/meminfo").split('\n')) {const auto f=line.simplified().split(' ');if(f.size()>1)memory[f[0]]=f[1].toLongLong()*1024;}
    const auto total=memory.value("MemTotal:");
    const auto available=memory.value("MemAvailable:",memory.value("MemFree:")+memory.value("Buffers:")+memory.value("Cached:"));
    if(total>0){s.values["memoryPercent"]=qBound(0,qRound(100.0*(total-available)/total),100);s.values["memoryText"]=bytes(total-available)+" / "+bytes(total);}
    const auto loads=read("/proc/loadavg").simplified().split(' ');
    if(loads.size()>=3)s.values["load"]=loads.mid(0,3).join(" / ");
    const auto uptime=read("/proc/uptime").section(' ',0,0).toDouble();
    const auto minutes=qint64(uptime)/60;
    s.values["uptime"]=QString("%1 天 %2 小时 %3 分钟").arg(minutes/1440).arg(minutes/60%24).arg(minutes%60);
    const QStorageInfo storage(QDir::homePath());
    if(storage.isValid()&&storage.isReady()&&storage.bytesTotal()>0){s.values["diskPercent"]=qRound(100.0*(storage.bytesTotal()-storage.bytesAvailable())/storage.bytesTotal());s.values["diskText"]=bytes(storage.bytesAvailable())+" 可用 / "+bytes(storage.bytesTotal());s.values["diskPath"]=storage.rootPath();}
    for(const auto &name:QDir("/sys/class/power_supply").entryList(QDir::Dirs|QDir::NoDotAndDotDot)) {
        const auto p="/sys/class/power_supply/"+name+"/";
        if(read(p+"type")!="Battery"||read(p+"present")=="0")continue;
        bool ok=false;const int capacity=read(p+"capacity").toInt(&ok);if(!ok)continue;
        const auto status=read(p+"status");
        s.values["batteryPresent"]=true;s.values["batteryPercent"]=qBound(0,capacity,100);
        s.values["batteryState"]=status=="Charging"?"充电中":status=="Discharging"?"使用电池":status=="Full"?"已充满":status=="Not charging"?"已接电源":"状态未知";
        break;
    }
#endif
    return s;
}
}
SystemStatus::SystemStatus(bool preview,QObject *parent):QObject(parent),m_preview(preview) {
    m_network={{"name","正在读取网络"},{"state","正在读取"},{"icon","network-offline"},{"connected",false},{"interfaces",QVariantList{}}};
    m_audio={{"available",false},{"volume",0},{"muted",false},{"text","未检测到音频输出"}};
    m_system={{"available",false},{"cpuPercent",-1},{"memoryPercent",-1},{"batteryPresent",false},{"host",QSysInfo::machineHostName()}};
    m_wpctl=QStandardPaths::findExecutable("wpctl");m_pactl=QStandardPaths::findExecutable("pactl");
    for(const auto &pair:QList<QPair<QString,QStringList>>{{"network",{"nm-connection-editor"}},{"audio",{"pavucontrol"}},{"monitor",{"plasma-systemmonitor","gnome-system-monitor","xfce4-taskmanager"}}}) {
        QString program;for(const auto &candidate:pair.second){program=QStandardPaths::findExecutable(candidate);if(!program.isEmpty())break;}
        m_tools[pair.first]=preview?QString():program;
    }
    m_clockTimer.setSingleShot(true);connect(&m_clockTimer,&QTimer::timeout,this,&SystemStatus::updateClock);updateClock();
    m_sampleTimer.setInterval(5000);m_sampleTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_sampleTimer,&QTimer::timeout,this,[this]{sampleSystem();if(!m_audioEvents||m_audioEvents->state()!=QProcess::Running)refreshAudio();});
    m_networkDebounce.setSingleShot(true);m_networkDebounce.setInterval(250);connect(&m_networkDebounce,&QTimer::timeout,this,&SystemStatus::refreshNetwork);
    m_audioDebounce.setSingleShot(true);m_audioDebounce.setInterval(150);connect(&m_audioDebounce,&QTimer::timeout,this,&SystemStatus::refreshAudio);
    m_volumeDebounce.setSingleShot(true);m_volumeDebounce.setInterval(180);connect(&m_volumeDebounce,&QTimer::timeout,this,&SystemStatus::flushVolume);
    if(preview){m_network["name"]="独立预览";m_network["state"]="不读取或修改主机状态";return;}
#ifdef SESSION_ACTIONS_DBUS
    auto bus=QDBusConnection::systemBus();
    bus.connect("org.freedesktop.NetworkManager","/org/freedesktop/NetworkManager","org.freedesktop.DBus.Properties","PropertiesChanged",this,SLOT(networkPropertiesChanged(QString,QVariantMap,QStringList)));
    auto *watcher=new QDBusServiceWatcher("org.freedesktop.NetworkManager",bus,QDBusServiceWatcher::WatchForOwnerChange,this);
    connect(watcher,&QDBusServiceWatcher::serviceOwnerChanged,this,[this]{m_networkDebounce.start();});
#endif
#ifdef Q_OS_LINUX
    m_routeFd=::socket(AF_NETLINK,SOCK_RAW|SOCK_NONBLOCK|SOCK_CLOEXEC,NETLINK_ROUTE);
    if(m_routeFd>=0){sockaddr_nl address{};address.nl_family=AF_NETLINK;address.nl_groups=RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR|RTMGRP_IPV4_ROUTE|RTMGRP_IPV6_ROUTE;
        if(::bind(m_routeFd,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0){m_routeEvents=new QSocketNotifier(m_routeFd,QSocketNotifier::Read,this);connect(m_routeEvents,&QSocketNotifier::activated,this,[this]{char buffer[8192];while(::recv(m_routeFd,buffer,sizeof(buffer),MSG_DONTWAIT)>0){}m_networkDebounce.start();});}
        else{::close(m_routeFd);m_routeFd=-1;}
    }
#endif
    refreshNetwork();refreshAudio();sampleSystem();startAudioEvents();
}
SystemStatus::~SystemStatus(){if(m_audioEvents)m_audioEvents->kill();delete m_routeEvents;
#ifdef Q_OS_LINUX
    if(m_routeFd>=0)::close(m_routeFd);
#endif
}
void SystemStatus::updateClock(){
    const auto now=QDateTime::currentDateTime();const QLocale locale;
    m_clock={{"time",locale.toString(now.time(),"HH:mm")},{"date",locale.toString(now.date(),"MM-dd ddd")},{"fullDate",locale.toString(now.date(),QLocale::LongFormat)},{"isoDate",now.date().toString(Qt::ISODate)},{"year",now.date().year()},{"month",now.date().month()},{"timezone",QString::fromUtf8(QTimeZone::systemTimeZoneId())}};
    emit clockChanged();m_clockTimer.start(60000-now.time().second()*1000-now.time().msec());
}
QStringList SystemStatus::weekdays()const{QStringList list;QLocale locale;int first=int(locale.firstDayOfWeek());for(int i=0;i<7;++i)list.append(locale.dayName((first+i-1)%7+1,QLocale::ShortFormat));return list;}
QVariantList SystemStatus::calendarDays(int year,int month)const{
    QVariantList result;const QDate first(year,month,1);if(!first.isValid())return result;
    const int offset=(first.dayOfWeek()-int(QLocale().firstDayOfWeek())+7)%7;
    for(int i=0;i<42;++i){const auto date=first.addDays(i-offset);result.append(QVariantMap{{"day",date.day()},{"date",date.toString(Qt::ISODate)},{"label",QLocale().toString(date,QLocale::LongFormat)},{"inMonth",date.month()==month},{"today",date==QDate::currentDate()},{"weekend",date.dayOfWeek()>=6}});}
    return result;
}
QString SystemStatus::calendarTitle(int year,int month)const{return QLocale().toString(QDate(year,month,1),"yyyy MMMM");}
void SystemStatus::setPanelOpen(bool open){if(m_panelOpen==open)return;m_panelOpen=open;emit panelOpenChanged();if(open){m_cpuTotal=m_cpuIdle=0;refresh();startAudioEvents();m_sampleTimer.start();QTimer::singleShot(1000,this,[this]{if(m_panelOpen)sampleSystem();});}else m_sampleTimer.stop();}
void SystemStatus::refresh(){if(m_preview)return;updateClock();refreshNetwork();refreshAudio();sampleSystem();}
void SystemStatus::sampleSystem(){
    if(m_preview||m_statsBusy)return;m_statsBusy=true;
    auto *watcher=new QFutureWatcher<Sample>(this);
    connect(watcher,&QFutureWatcher<Sample>::finished,this,[this,watcher]{const auto sample=watcher->result();m_statsBusy=false;m_cpuTotal=sample.total;m_cpuIdle=sample.idle;if(m_system!=sample.values){m_system=sample.values;emit systemChanged();}watcher->deleteLater();});
    watcher->setFuture(QtConcurrent::run(hostSample,m_cpuTotal,m_cpuIdle));
}
void SystemStatus::networkPropertiesChanged(const QString &,const QVariantMap &,const QStringList &){m_networkDebounce.start();}
void SystemStatus::refreshNetwork(){
    if(m_preview)return;if(m_networkBusy){m_networkDirty=true;return;}
    auto base=localNetwork();
#ifdef SESSION_ACTIONS_DBUS
    m_networkBusy=true;const auto generation=++m_networkGeneration;
    auto apply=[this,generation](const QVariantMap &values){if(generation!=m_networkGeneration)return;m_networkBusy=false;if(m_network!=values){m_network=values;emit networkChanged();}if(m_networkDirty){m_networkDirty=false;m_networkDebounce.start();}};
    auto message=QDBusMessage::createMethodCall("org.freedesktop.NetworkManager","/org/freedesktop/NetworkManager","org.freedesktop.DBus.Properties","GetAll");message<<QString("org.freedesktop.NetworkManager");
    auto *reply=new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message,3000),this);
    connect(reply,&QDBusPendingCallWatcher::finished,this,[this,base,apply](QDBusPendingCallWatcher *watcher)mutable{
        QDBusPendingReply<QVariantMap> result=*watcher;watcher->deleteLater();if(result.isError()){apply(base);return;}
        const auto fields=result.value();const auto path=fields.value("PrimaryConnection").value<QDBusObjectPath>().path();
        const auto type=fields.value("PrimaryConnectionType").toString();const int state=fields.value("State").toInt();
        // Unmanaged VPN/network interfaces remain visible in the kernel fallback.
        if(state>=50){base["connected"]=true;base["name"]=type=="802-11-wireless"?"无线网络":"网络连接";base["icon"]=type=="802-11-wireless"?"network-wireless":"network-wired";
            const bool checked=fields.value("ConnectivityCheckAvailable").toBool()&&fields.value("ConnectivityCheckEnabled").toBool();
            const int connectivity=fields.value("Connectivity").toInt();
            base["state"]=!checked?"已连接 · 互联网状态未检测":connectivity==4?"互联网可用（NetworkManager 检测）":connectivity==2?"需要网页登录":connectivity==3?"网络连接受限":connectivity==1?"未检测到互联网连接":"已连接 · 连通性未知";
        }else if(!base.value("connected").toBool())base["state"]=state>=40?"正在连接网络":"网络未连接";
        base["source"]="NetworkManager / 系统接口";
        auto bus=QDBusConnection::systemBus();
        if(m_primaryPath!=path){if(!m_primaryPath.isEmpty())bus.disconnect("org.freedesktop.NetworkManager",m_primaryPath,"org.freedesktop.DBus.Properties","PropertiesChanged",this,SLOT(networkPropertiesChanged(QString,QVariantMap,QStringList)));m_primaryPath=path;if(!path.isEmpty()&&path!="/")bus.connect("org.freedesktop.NetworkManager",path,"org.freedesktop.DBus.Properties","PropertiesChanged",this,SLOT(networkPropertiesChanged(QString,QVariantMap,QStringList)));}
        if(path.isEmpty()||path=="/"){apply(base);return;}
        auto msg=QDBusMessage::createMethodCall("org.freedesktop.NetworkManager",path,"org.freedesktop.DBus.Properties","GetAll");msg<<QString("org.freedesktop.NetworkManager.Connection.Active");
        auto *active=new QDBusPendingCallWatcher(bus.asyncCall(msg,3000),this);
        connect(active,&QDBusPendingCallWatcher::finished,this,[base,apply](QDBusPendingCallWatcher *watcher)mutable{QDBusPendingReply<QVariantMap> connection=*watcher;watcher->deleteLater();if(!connection.isError()&&!connection.value().value("Id").toString().isEmpty())base["name"]=connection.value().value("Id").toString();apply(base);});
    });
#else
    if(m_network!=base){m_network=base;emit networkChanged();}
#endif
}
void SystemStatus::run(const QString &program,const QStringList &args,std::function<void(bool,const QString &)> callback){
    auto *process=new QProcess(this);auto environment=QProcessEnvironment::systemEnvironment();environment.insert("LC_ALL","C");process->setProcessEnvironment(environment);
    auto *timeout=new QTimer(process);timeout->setSingleShot(true);
    auto completed=std::make_shared<bool>(false);
    auto finish=[process,timeout,completed,callback](bool ok,const QString &output){if(*completed)return;*completed=true;timeout->stop();callback(ok,output.trimmed().left(8192));process->deleteLater();};
    connect(process,&QProcess::errorOccurred,this,[process,finish](QProcess::ProcessError error){if(error==QProcess::FailedToStart)finish(false,process->errorString());});
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[process,finish](int code,QProcess::ExitStatus status){finish(code==0&&status==QProcess::NormalExit,QString::fromUtf8(code==0?process->readAllStandardOutput():process->readAllStandardError()));});
    connect(timeout,&QTimer::timeout,this,[process]{process->kill();});timeout->start(3000);process->start(program,args);
}
void SystemStatus::refreshAudio() {
    if (m_preview) return;
    if (m_audioBusy) { m_audioDirty = true; return; }
    m_audioBusy = true;
    const auto apply = [this](bool ok, int volume, bool muted, const QString &program) {
        QVariantMap state{{"available", ok}, {"volume", qBound(0, volume, 100)},
            {"muted", muted}, {"text", !ok ? "未检测到音频输出" : muted
                ? QString("已静音 · %1%").arg(volume) : QString("音量 %1%").arg(volume)}};
        m_audioProgram = ok ? program : QString();
        m_audioBusy = false;
        if (state != m_audio) { m_audio = state; emit audioChanged(); }
        if (m_audioDirty) { m_audioDirty = false; m_audioDebounce.start(); }
    };
    const auto readPactl = [this, apply] {
        if (m_pactl.isEmpty()) { apply(false, 0, false, {}); return; }
        run(m_pactl, {"get-sink-volume", "@DEFAULT_SINK@"}, [this, apply](bool ok, const QString &text) {
            const auto match = QRegularExpression("(\\d+)%").match(text);
            if (!ok || !match.hasMatch()) { apply(false, 0, false, {}); return; }
            const int volume = match.captured(1).toInt();
            run(m_pactl, {"get-sink-mute", "@DEFAULT_SINK@"}, [this, apply, volume](bool ok, const QString &text) {
                apply(ok, volume, text.contains("yes"), m_pactl);
            });
        });
    };
    // A remote session may use its own PulseAudio server. Keep both reads and
    // controls on that server instead of silently controlling host PipeWire.
    if (!qEnvironmentVariable("PULSE_SERVER").isEmpty() || m_wpctl.isEmpty()) readPactl();
    else run(m_wpctl, {"get-volume", "@DEFAULT_AUDIO_SINK@"}, [this, apply, readPactl](bool ok, const QString &text) {
        const auto match = QRegularExpression("Volume:\\s*([0-9.]+)").match(text);
        if (!ok || !match.hasMatch()) { readPactl(); return; }
        apply(true, qRound(match.captured(1).toDouble() * 100), text.contains("[MUTED]"), m_wpctl);
    });
}
void SystemStatus::startAudioEvents(){
    if(m_preview||m_pactl.isEmpty())return;
    if(m_audioEvents&&m_audioEvents->state()!=QProcess::NotRunning)return;
    if(!m_audioEvents){m_audioEvents=new QProcess(this);auto env=QProcessEnvironment::systemEnvironment();env.insert("LC_ALL","C");m_audioEvents->setProcessEnvironment(env);
        connect(m_audioEvents,&QProcess::readyReadStandardOutput,this,[this]{auto buffer=m_audioEvents->property("buffer").toByteArray()+m_audioEvents->readAllStandardOutput();int end;while((end=buffer.indexOf('\n'))>=0){const auto line=buffer.left(end);buffer.remove(0,end+1);if(line.contains(" on sink ")||line.contains(" on server "))m_audioDebounce.start();}m_audioEvents->setProperty("buffer",buffer.right(4096));});
        connect(m_audioEvents,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this]{m_audioDebounce.start();});
    }
    m_audioEvents->start(m_pactl,{"subscribe"});
}
void SystemStatus::setVolume(int percent){if(m_preview||!m_audio.value("available").toBool())return;m_pendingVolume=qBound(0,percent,100);m_volumeDebounce.start();}
void SystemStatus::flushVolume(){
    if(m_pendingVolume<0||m_audioControlBusy)return;
    if(m_audioProgram.isEmpty()){m_pendingVolume=-1;return;}
    const int volume=m_pendingVolume;m_pendingVolume=-1;m_audioControlBusy=true;
    const QString program=m_audioProgram;
    const QStringList args=m_audioProgram==m_wpctl?QStringList{"set-volume","@DEFAULT_AUDIO_SINK@",QString::number(volume)+"%","--limit","1.0"}:QStringList{"set-sink-volume","@DEFAULT_SINK@",QString::number(volume)+"%"};
    run(program,args,[this](bool ok,const QString &){m_audioControlBusy=false;if(!ok)emit failure("无法调整音量，请检查当前会话的音频服务。");refreshAudio();if(m_pendingVolume>=0)m_volumeDebounce.start();});
}
void SystemStatus::toggleMute(){
    if(m_preview||!m_audio.value("available").toBool()||m_audioControlBusy)return;m_audioControlBusy=true;
    run(m_audioProgram,m_audioProgram==m_wpctl?QStringList{"set-mute","@DEFAULT_AUDIO_SINK@","toggle"}:QStringList{"set-sink-mute","@DEFAULT_SINK@","toggle"},[this](bool ok,const QString &){m_audioControlBusy=false;if(!ok)emit failure("无法切换静音，请检查当前会话的音频服务。");refreshAudio();if(m_pendingVolume>=0)m_volumeDebounce.start();});
}
void SystemStatus::openTool(const QString &key){if(m_preview)return;const auto program=m_tools.value(key).toString();if(program.isEmpty()){emit failure("此工具未安装。");return;}if(!QProcess::startDetached(program,{}))emit failure("未能打开工具，请检查安装状态。");}
