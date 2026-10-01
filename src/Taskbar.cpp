#include "Taskbar.h"
#include <QTimer>
#include <QGuiApplication>
#include <QSocketNotifier>
#include <QHash>
#include <QFileInfo>
#ifdef XDOCK_X11
#include <xcb/xcb.h>
#include <cstdlib>
#include <cstring>
namespace {
xcb_atom_t atom(xcb_connection_t *c,const char *name) {
 static QHash<QByteArray, xcb_atom_t> cache;
 if(cache.contains(name)) return cache.value(name);
 auto *r=xcb_intern_atom_reply(c,xcb_intern_atom(c,0,std::strlen(name),name),nullptr);
 auto a=r?r->atom:0; std::free(r); cache.insert(name,a); return a;
}
QByteArray readProperty(xcb_connection_t *c,quint32 w,const char *name) {
 auto *r=xcb_get_property_reply(c,xcb_get_property(c,0,w,atom(c,name),XCB_GET_PROPERTY_TYPE_ANY,0,65536),nullptr);
 QByteArray b; if(r) b=QByteArray(static_cast<const char*>(xcb_get_property_value(r)),xcb_get_property_value_length(r));
 std::free(r); return b;
}
QList<quint32> values(const QByteArray &b) {
 QList<quint32> out; for(int i=0;i+4<=b.size();i+=4) { quint32 n; std::memcpy(&n,b.constData()+i,4); out.append(n); } return out;
}
}
#endif
Taskbar::Taskbar(QObject *parent):QObject(parent) {
#ifdef XDOCK_X11
 if(QGuiApplication::platformName()=="xcb") {
  auto *c=xcb_connect(nullptr,nullptr);
  if(!xcb_connection_has_error(c)) { m_connection=c; m_root=xcb_setup_roots_iterator(xcb_get_setup(c)).data->root; }
  else xcb_disconnect(c);
 }
#endif
#ifdef XDOCK_X11
 if(m_connection) {
  auto *c=static_cast<xcb_connection_t*>(m_connection);
  quint32 mask=XCB_EVENT_MASK_PROPERTY_CHANGE|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
  xcb_change_window_attributes(c,m_root,XCB_CW_EVENT_MASK,&mask); xcb_flush(c);
  auto *debounce=new QTimer(this); debounce->setSingleShot(true); debounce->setInterval(60);
  connect(debounce,&QTimer::timeout,this,&Taskbar::refresh);
  auto *notifier=new QSocketNotifier(xcb_get_file_descriptor(c),QSocketNotifier::Read,this);
  connect(notifier,&QSocketNotifier::activated,this,[c,debounce] {
   bool dirty=false;
   while(auto *event=xcb_poll_for_event(c)) {
    auto type=event->response_type&~0x80;
    dirty |= type==XCB_PROPERTY_NOTIFY||type==XCB_DESTROY_NOTIFY||type==XCB_MAP_NOTIFY||type==XCB_UNMAP_NOTIFY;
    std::free(event);
   }
   if(dirty&&!debounce->isActive())debounce->start();
  });
  // Recovery only: normal updates come from PropertyNotify and root events.
  auto *fallback=new QTimer(this); fallback->setInterval(15000);
  connect(fallback,&QTimer::timeout,this,&Taskbar::refresh); fallback->start(); refresh();
 }
#endif
}
Taskbar::~Taskbar() {
#ifdef XDOCK_X11
 if(m_connection) xcb_disconnect(static_cast<xcb_connection_t*>(m_connection));
#endif
}
void Taskbar::refresh() {
#ifdef XDOCK_X11
 auto *c=static_cast<xcb_connection_t*>(m_connection); if(!c)return;
 auto scalar=[&](const char *name,quint32 fallback) { auto v=values(readProperty(c,m_root,name)); return v.isEmpty()?fallback:v.first(); };
 m_active=scalar("_NET_ACTIVE_WINDOW",0);
 int count=qBound(1,int(scalar("_NET_NUMBER_OF_DESKTOPS",1)),32), current=int(scalar("_NET_CURRENT_DESKTOP",0));
 m_showing=scalar("_NET_SHOWING_DESKTOP",0);
 QVariantList list;
 auto skip=atom(c,"_NET_WM_STATE_SKIP_TASKBAR"), hidden=atom(c,"_NET_WM_STATE_HIDDEN"), dock=atom(c,"_NET_WM_WINDOW_TYPE_DOCK"), desktop=atom(c,"_NET_WM_WINDOW_TYPE_DESKTOP");
 for(auto id:values(readProperty(c,m_root,"_NET_CLIENT_LIST"))) {
  quint32 mask=XCB_EVENT_MASK_PROPERTY_CHANGE; xcb_change_window_attributes(c,id,XCB_CW_EVENT_MASK,&mask);
  auto states=values(readProperty(c,id,"_NET_WM_STATE")),types=values(readProperty(c,id,"_NET_WM_WINDOW_TYPE"));
  if(states.contains(skip)||types.contains(dock)||types.contains(desktop))continue;
  auto title=QString::fromUtf8(readProperty(c,id,"_NET_WM_NAME")); if(title.isEmpty())title=QString::fromLocal8Bit(readProperty(c,id,"WM_NAME")); if(title.isEmpty())continue;
  auto ds=values(readProperty(c,id,"_NET_WM_DESKTOP")); quint32 d=ds.isEmpty()?current:ds.first();
  auto classes=readProperty(c,id,"WM_CLASS").split('\0');
  if(QString::fromUtf8(classes.value(1)).compare("XLaunch",Qt::CaseInsensitive)==0 || QString::fromUtf8(classes.value(1)).compare("XDock",Qt::CaseInsensitive)==0)continue;
  auto pid=values(readProperty(c,id,"_NET_WM_PID"));
  QString executable=pid.isEmpty()?QString():QFileInfo(QFileInfo(QString("/proc/%1/exe").arg(pid.first())).symLinkTarget()).fileName();
  list.append(QVariantMap{{"wmClass",QString::fromUtf8(classes.value(1))},{"instance",QString::fromUtf8(classes.value(0))},{"appId",QString::fromUtf8(readProperty(c,id,"_GTK_APPLICATION_ID"))},{"executable",executable},{"id",id},{"title",title},{"active",id==m_active},{"minimized",states.contains(hidden)},{"desktop",d}});
 }
 xcb_flush(c);
 if(list!=m_windows||count!=m_count||current!=m_current) { m_windows=list;m_count=count;m_current=current;emit changed(); }
#endif
}
void Taskbar::send(quint32 w,const char *name,quint32 a,quint32 b,quint32 d) {
#ifdef XDOCK_X11
 auto *c=static_cast<xcb_connection_t*>(m_connection);if(!c)return;
 xcb_client_message_event_t e{};e.response_type=XCB_CLIENT_MESSAGE;e.format=32;e.window=w;e.type=atom(c,name);e.data.data32[0]=a;e.data.data32[1]=b;e.data.data32[2]=d;
 xcb_send_event(c,0,m_root,XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,reinterpret_cast<const char*>(&e));xcb_flush(c);
#else
 Q_UNUSED(w);Q_UNUSED(name);Q_UNUSED(a);Q_UNUSED(b);Q_UNUSED(d);
#endif
}
void Taskbar::activate(quint32 id) {
 refresh();
 for(const auto &v:m_windows) { auto m=v.toMap();if(m["id"].toUInt()==id && m["desktop"].toUInt()!=0xffffffffU) switchDesktop(m["desktop"].toInt()); }
 send(id,"_NET_ACTIVE_WINDOW",2,0,m_active);
}
void Taskbar::minimize(quint32 id) { send(id,"WM_CHANGE_STATE",3); }
void Taskbar::closeWindow(quint32 id) { send(id,"_NET_CLOSE_WINDOW",0,2); }
void Taskbar::switchDesktop(int d) { if(d>=0&&d<m_count)send(m_root,"_NET_CURRENT_DESKTOP",d); }
void Taskbar::showDesktop() { refresh();send(m_root,"_NET_SHOWING_DESKTOP",!m_showing); }

void Taskbar::maximize(quint32 id) {
#ifdef XDOCK_X11
 auto *c=static_cast<xcb_connection_t*>(m_connection); if(c) send(id,"_NET_WM_STATE",2,atom(c,"_NET_WM_STATE_MAXIMIZED_VERT"),atom(c,"_NET_WM_STATE_MAXIMIZED_HORZ"));
#else
 Q_UNUSED(id);
#endif
}
