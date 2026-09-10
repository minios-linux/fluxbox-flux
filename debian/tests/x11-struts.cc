#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

static Display *dpy;
static Window root;
static int checks = 0;
static Atom atom(const char *s) { return XInternAtom(dpy, s, False); }
static void fail(const std::string &s) { std::cerr << "FAIL " << s << '\n'; std::exit(1); }
static void pass(const char *s) { ++checks; std::cout << "PASS X11: " << s << std::endl; }
static std::vector<unsigned long> get(Window w, const char *name) {
    Atom type; int format; unsigned long n, rest; unsigned char *data=0;
    std::vector<unsigned long> result;
    if (XGetWindowProperty(dpy,w,atom(name),0,4096,False,AnyPropertyType,
                          &type,&format,&n,&rest,&data)==Success && data && format==32) {
        unsigned long *p=reinterpret_cast<unsigned long *>(data);
        result.assign(p,p+n);
    }
    if (data) XFree(data);
    return result;
}
static void put(Window w, const char *name, const unsigned long *v, int n,
                Atom type=XA_CARDINAL, int format=32) {
    XChangeProperty(dpy,w,atom(name),type,format,PropModeReplace,
                    reinterpret_cast<const unsigned char *>(v),n);
    XSync(dpy,False);
}
static void erase(Window w, const char *name) { XDeleteProperty(dpy,w,atom(name)); XSync(dpy,False); }
// A reply to a later ClientMessage is a WM-side barrier. XSync alone only
// waits for the X server and could falsely pass an unchanged-area assertion.
static void settle() {
    const std::vector<unsigned long> old = get(root, "_NET_NUMBER_OF_DESKTOPS");
    const long count = !old.empty() && old[0] == 2 ? 3 : 2;
    XEvent e = {}; e.xclient.type = ClientMessage; e.xclient.window = root;
    e.xclient.message_type = atom("_NET_NUMBER_OF_DESKTOPS"); e.xclient.format = 32;
    e.xclient.data.l[0] = count;
    XSendEvent(dpy, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &e);
    XSync(dpy, False);
    for (int i = 0; i < 500; ++i) {
        const std::vector<unsigned long> v = get(root, "_NET_NUMBER_OF_DESKTOPS");
        if (!v.empty() && v[0] == static_cast<unsigned long>(count)) return;
        usleep(10000);
    }
    fail("window-manager barrier");
}
static void area(const char *label, int x, int y, int w, int h) {
    settle();
    const unsigned long want[4]={static_cast<unsigned long>(x),static_cast<unsigned long>(y),
                                static_cast<unsigned long>(w),static_cast<unsigned long>(h)};
    for (int attempt=0;attempt<500;++attempt) {
        const std::vector<unsigned long> v=get(root,"_NET_WORKAREA");
        bool equal = !v.empty() && v.size() % 4 == 0;
        for (size_t i = 0; equal && i < v.size(); i += 4)
            equal = std::equal(want, want + 4, v.begin() + i);
        if (equal) { pass(label); return; }
        usleep(10000);
    }
    const std::vector<unsigned long> v=get(root,"_NET_WORKAREA");
    for (size_t i=0;i<v.size();++i) std::cerr << v[i] << ',';
    fail(label);
}
static Window dock() {
    Window w=XCreateSimpleWindow(dpy,root,0,747,1280,53,0,0,0);
    unsigned long type=atom("_NET_WM_WINDOW_TYPE_DOCK");
    put(w,"_NET_WM_WINDOW_TYPE",&type,1,XA_ATOM);
    return w;
}
static void state(Window w, int action, const char *first, const char *second=0) {
    XEvent event={}; event.xclient.type=ClientMessage; event.xclient.window=w;
    event.xclient.message_type=atom("_NET_WM_STATE"); event.xclient.format=32;
    event.xclient.data.l[0]=action; event.xclient.data.l[1]=atom(first);
    event.xclient.data.l[2]=second ? atom(second):0; event.xclient.data.l[3]=2;
    XSendEvent(dpy,root,False,SubstructureNotifyMask|SubstructureRedirectMask,&event);
    XSync(dpy,False);
}
static void geometry(Window w, const char *label, int x, int y, int width, int height) {
    for (int attempt=0;attempt<500;++attempt) {
        XWindowAttributes a; Window child; int rx,ry;
        XGetWindowAttributes(dpy,w,&a); XTranslateCoordinates(dpy,w,root,0,0,&rx,&ry,&child);
        const std::vector<unsigned long> f=get(w,"_NET_FRAME_EXTENTS");
        if (f.size()==4 && rx-static_cast<int>(f[0])==x && ry-static_cast<int>(f[2])==y
            && a.width+f[0]+f[1]==static_cast<unsigned long>(width)
            && a.height+f[2]+f[3]==static_cast<unsigned long>(height)) { pass(label); return; }
        usleep(10000);
    }
    fail(label);
}
int main() {
    dpy=XOpenDisplay(0); if (!dpy) fail("XOpenDisplay"); root=DefaultRootWindow(dpy);
    bool ready=false;
    for(int i=0;i<500;++i) {
        const std::vector<unsigned long> v=get(root,"_NET_SUPPORTED");
        if(std::find(v.begin(),v.end(),atom("_NET_WM_STRUT_PARTIAL"))!=v.end()) { ready=true; break; }
        usleep(10000);
    }
    if(!ready) fail("window manager does not advertise PARTIAL");
    area("initial area",0,0,1280,800);
    Window panel=dock();
    unsigned long v[13]={0,0,0,53,0,0,0,0,0,0,0,1279,0};
    put(panel,"_NET_WM_STRUT_PARTIAL",v,12); XMapWindow(dpy,panel); XSync(dpy,False);
    area("strut before mapping",0,0,1280,747);
    Window win=XCreateSimpleWindow(dpy,root,100,100,400,300,0,0,0);
    XMapWindow(dpy,win); XSync(dpy,False);
    for(int i=0;i<500 && get(win,"_NET_FRAME_EXTENTS").empty();++i) usleep(10000);
    state(win,1,"_NET_WM_STATE_MAXIMIZED_HORZ","_NET_WM_STATE_MAXIMIZED_VERT");
    geometry(win,"maximized window excludes panel",0,0,1280,747);
    state(win,1,"_NET_WM_STATE_FULLSCREEN");
    geometry(win,"fullscreen uses entire screen",0,0,1280,800);
    state(win,0,"_NET_WM_STATE_FULLSCREEN");
    geometry(win,"fullscreen restores maximization",0,0,1280,747);
    v[3]=65; put(panel,"_NET_WM_STRUT_PARTIAL",v,12);
    area("changed partial",0,0,1280,735);
    geometry(win,"already maximized window follows panel resize",0,0,1280,735);
    unsigned long legacy[4]={0,0,0,100}; put(panel,"_NET_WM_STRUT",legacy,4);
    area("partial preferred over legacy",0,0,1280,735);
    v[3]=0; put(panel,"_NET_WM_STRUT_PARTIAL",v,12);
    area("zero partial overrides legacy",0,0,1280,800);
    erase(panel,"_NET_WM_STRUT_PARTIAL"); area("deleted partial falls back to legacy",0,0,1280,700);
    put(panel,"_NET_WM_STRUT_PARTIAL",v,4); area("short partial falls back",0,0,1280,700);
    erase(panel,"_NET_WM_STRUT"); area("invalid partial alone releases space",0,0,1280,800);
    v[3]=53; put(panel,"_NET_WM_STRUT_PARTIAL",v,13); area("extra items rejected",0,0,1280,800);
    put(panel,"_NET_WM_STRUT_PARTIAL",v,12,XA_STRING); area("wrong type rejected",0,0,1280,800);
    put(panel,"_NET_WM_STRUT_PARTIAL",v,12,XA_CARDINAL,16); area("wrong format rejected",0,0,1280,800);
    put(panel,"_NET_WM_STRUT",legacy,3); area("short legacy rejected",0,0,1280,800);
    erase(panel,"_NET_WM_STRUT");
    put(panel,"_NET_WM_STRUT_PARTIAL",v,12); area("valid property after invalid input",0,0,1280,747);
    Window second=dock(); unsigned long top[12]={0,0,30,0,0,0,0,0,0,1279,0,0};
    put(second,"_NET_WM_STRUT_PARTIAL",top,12); XMapWindow(dpy,second); XSync(dpy,False);
    area("two independent panels",0,30,1280,717);
    XDestroyWindow(dpy,second); XSync(dpy,False); area("destroy second panel",0,0,1280,747);
    for(int i=0;i<100;++i) { v[3]=(i%2)?53:65; put(panel,"_NET_WM_STRUT_PARTIAL",v,12); }
    area("repeated updates do not accumulate",0,0,1280,747);
    v[10]=1280; v[11]=2559; put(panel,"_NET_WM_STRUT_PARTIAL",v,12);
    area("nonintersecting span ignored",0,0,1280,800);
    v[10]=800; v[11]=799; put(panel,"_NET_WM_STRUT_PARTIAL",v,12);
    area("reversed span ignored",0,0,1280,800);
    erase(panel,"_NET_WM_STRUT_PARTIAL"); area("property removal releases area",0,0,1280,800);
    v[10]=0; v[11]=1279; put(panel,"_NET_WM_STRUT_PARTIAL",v,12);
    area("panel reservation restored",0,0,1280,747);
    XDestroyWindow(dpy,panel); XSync(dpy,False); area("destroy panel releases area",0,0,1280,800);
    panel=dock(); put(panel,"_NET_WM_STRUT_PARTIAL",v,12); XMapWindow(dpy,panel); XSync(dpy,False);
    area("recreated panel reserves area",0,0,1280,747);
    XUnmapWindow(dpy,panel); XSync(dpy,False); area("unmapped panel releases area",0,0,1280,800);
    XMapWindow(dpy,panel); XSync(dpy,False); area("remapped panel reserves area",0,0,1280,747);
    unsigned long sides[12] = {20,30,40,50,0,799,0,799,0,1279,0,1279};
    put(panel,"_NET_WM_STRUT_PARTIAL",sides,12);
    area("all four screen edges",20,40,1230,710);
    geometry(win,"maximized frame follows all edges",20,40,1230,710);
    XDestroyWindow(dpy,panel); XDestroyWindow(dpy,win); XSync(dpy,False);
    area("final cleanup",0,0,1280,800);
    XCloseDisplay(dpy); std::cout << "X11 tests: " << checks << " passed\n";
}
