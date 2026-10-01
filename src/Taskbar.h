#pragma once
#include <QObject>
#include <QVariantList>
class Taskbar : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantList windows READ windows NOTIFY changed)
 Q_PROPERTY(int desktopCount READ desktopCount NOTIFY changed)
 Q_PROPERTY(int currentDesktop READ currentDesktop NOTIFY changed)
public:
 explicit Taskbar(QObject *parent=nullptr);
 ~Taskbar();
 QVariantList windows() const { return m_windows; }
 int desktopCount() const { return m_count; }
 int currentDesktop() const { return m_current; }
 Q_INVOKABLE void activate(quint32 id);
 Q_INVOKABLE void minimize(quint32 id);
 Q_INVOKABLE void maximize(quint32 id);
 Q_INVOKABLE void closeWindow(quint32 id);
 Q_INVOKABLE void switchDesktop(int desktop);
 Q_INVOKABLE void showDesktop();
signals:
 void changed();
private:
 void refresh();
 void send(quint32 window, const char *name, quint32 a=0, quint32 b=0, quint32 c=0);
 void *m_connection=nullptr;
 quint32 m_root=0, m_active=0;
 int m_count=1,m_current=0;
 bool m_showing=false;
 QVariantList m_windows;
};
