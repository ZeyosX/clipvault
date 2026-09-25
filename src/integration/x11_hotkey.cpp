#include "integration/x11_hotkey.h"
#include "util/hotkey_parse.h"
#include <QGuiApplication>
#include <QSocketNotifier>
#if CLIPVAULT_HAVE_X11
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif
namespace cv {
    X11Hotkey::X11Hotkey(QObject *parent) : QObject(parent) {
    }
    X11Hotkey::~X11Hotkey() { cleanup(); }
    bool X11Hotkey::start(const QString &hotkey) {
#if !CLIPVAULT_HAVE_X11
        emit status("X11 support not built");
        return false;
#else
        if (QGuiApplication::platformName() != "xcb") {
            emit status("Not on X11/xcb platform");
            return false;
        }
        Display *dpy = XOpenDisplay(nullptr);
        if (!dpy) {
            emit status("XOpenDisplay failed");
            return false;
        }
        _dpy = dpy;
        _root = DefaultRootWindow(dpy);
        const auto [ok, mods, key, error] = hotkey::parseForX11(hotkey);
        if (!ok) {
            emit status("Hotkey parse error: " + error);
            cleanup();
            return false;
        }
        _mods = mods;
        const auto keyName = key.toUtf8();
        const KeySym ks = XStringToKeysym(keyName.constData());
        if (ks == NoSymbol) {
            emit status("Unknown key: " + key);
            cleanup();
            return false;
        }
        _keycode = XKeysymToKeycode(dpy, ks);
        if (_keycode == 0) {
            emit status("Could not find keycode for F1");
            cleanup();
            return false;
        }
        const unsigned int base = _mods;
        constexpr unsigned int modifiers[] = {
            0,
            LockMask,
            Mod2Mask,
            LockMask | Mod2Mask
        };
        for (const unsigned int m: modifiers) {
            XGrabKey(dpy, static_cast<int>(_keycode), base | m, _root, True, GrabModeAsync, GrabModeAsync);
        }
        XSelectInput(dpy, _root, KeyPressMask);
        XSync(dpy, False);
        _notifier = new QSocketNotifier(ConnectionNumber(dpy), QSocketNotifier::Read, this);
        connect(_notifier, &QSocketNotifier::activated, this, &X11Hotkey::onX11Readable);
        emit status(QString("X11 hotkey ready (%1)").arg(hotkey));
        return true;
#endif
    }
    void X11Hotkey::onX11Readable() {
#if CLIPVAULT_HAVE_X11
        auto *dpy = static_cast<Display *>(_dpy);
        if (!dpy) return;
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == KeyPress) {
                if (const auto &ke = ev.xkey; ke.keycode == _keycode && (ke.state & _mods) == _mods) {
                    emit activated();
                }
            }
        }
#endif
    }
    void X11Hotkey::stop() {
        cleanup();
    }
    void X11Hotkey::cleanup() {
#if CLIPVAULT_HAVE_X11
        if (_notifier) {
            _notifier->deleteLater();
            _notifier = nullptr;
        }
        if (_dpy) {
            auto *dpy = static_cast<Display *>(_dpy);
            if (_keycode) {
                constexpr unsigned int modifiers[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
                for (const unsigned int m: modifiers) {
                    XUngrabKey(dpy, static_cast<int>(_keycode), _mods | m, _root);
                }
            }
            XCloseDisplay(dpy);
            _dpy = nullptr;
        }
#endif
    }
}
