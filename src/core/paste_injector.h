#pragma once

#include <string>

class PasteInjector {
public:
    PasteInjector();
    ~PasteInjector();

    bool init();
    bool paste(int delay_ms = 35);

private:
    int uinput_fd_ = -1;
    void emitKey(int type, int code, int val);
};
