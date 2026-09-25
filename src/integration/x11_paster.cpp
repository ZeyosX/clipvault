#include "integration/x11_paster.h"
#include <QGuiApplication>
#include <QTimer>
#if CLIPVAULT_HAVE_X11
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#endif
namespace cv {
    X11Paster::X11Paster(QObject *parent) : QObject(parent) {
#if CLIPVAULT_HAVE_X11
        if (QGuiApplication::platformName() == "xcb") {
            _dpy = XOpenDisplay(nullptr);
        }
#endif
    }
    X11Paster::~X11Paster() {
#if CLIPVAULT_HAVE_X11
        if (_dpy) XCloseDisplay(static_cast<Display *>(_dpy));
#endif
    }
    bool X11Paster::isAvailable() const {
#if CLIPVAULT_HAVE_X11
        return _dpy != nullptr && QGuiApplication::platformName() == "xcb";
#else
        return false;
#endif
    }
    void X11Paster::pasteCtrlV() const {
#if !CLIPVAULT_HAVE_X11
        emit status("X11 support not built");
        return;
#else
        if (!isAvailable()) return;
        QTimer::singleShot(50, this, [this]() {
            auto *dpy = static_cast<Display *>(_dpy);
            if (!dpy) return;
            const KeyCode kcCtrl = XKeysymToKeycode(dpy, XK_Control_L);
            const KeyCode kcV = XKeysymToKeycode(dpy, XK_v);
            XTestFakeKeyEvent(dpy, kcCtrl, True, CurrentTime);
            XTestFakeKeyEvent(dpy, kcV, True, CurrentTime);
            XTestFakeKeyEvent(dpy, kcV, False, CurrentTime);
            XTestFakeKeyEvent(dpy, kcCtrl, False, CurrentTime);
            XFlush(dpy);
        });
#endif
    }
} 
