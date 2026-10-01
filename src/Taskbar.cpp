#include "Taskbar.h"
#include <QTimer>
#include <QGuiApplication>
#ifdef XDOCK_X11
#include <xcb/xcb.h>
#include <cstdlib>
#include <cstring>
namespace {
xcb_atom_t atom(xcb_connection_t *c,const char *name) {
 auto *r=xcb_intern_atom_reply(c,xcb_intern_atom(c,0,std::strlen(name),name),nullptr);
 auto a=r?r->atom:0; std::free(r); return a;
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
 if(m_connection) { auto *t=new QTimer(this); t->setInterval(1000); connect(t,&QTimer::timeout,this,&Taskbar::refresh); t->start(); refresh(); }
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
  auto states=values(readProperty(c,id,"_NET_WM_STATE")),types=values(readProperty(c,id,"_NET_WM_WINDOW_TYPE"));
  if(states.contains(skip)||types.contains(dock)||types.contains(desktop))continue;
  auto title=QString::fromUtf8(readProperty(c,id,"_NET_WM_NAME")); if(title.isEmpty())title=QString::fromLocal8Bit(readProperty(c,id,"WM_NAME")); if(title.isEmpty())continue;
  auto ds=values(readProperty(c,id,"_NET_WM_DESKTOP")); quint32 d=ds.isEmpty()?current:ds.first();
  list.append(QVariantMap{{"id",id},{"title",title},{"active",id==m_active},{"minimized",states.contains(hidden)},{"desktop",d}});
 }
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
