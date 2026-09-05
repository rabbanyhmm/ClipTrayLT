#pragma once

#include <QObject>
#include <QAbstractNativeEventFilter>

typedef struct _XDisplay Display;

class GlobalHotkey : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject* parent = nullptr);
    ~GlobalHotkey() override;

    bool registerHotkey();
    void unregisterHotkey();
    bool isRegistered() const { return registered_; }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void triggered();

private:
    Display* dpy_ = nullptr;
    unsigned int keycode_v_ = 0;
    bool registered_ = false;
};
