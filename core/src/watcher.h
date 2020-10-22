#pragma once

template <class T>
class watcher {
public:
    watcher(bool changed = false) {
        _changed = changed;
    }

    watcher(T value, bool changed = false) {
        val = value;
        _val = value;
        _changed = changed;
    }

    bool changed(bool clear = true) {
        bool ch = ((val != _val) || _changed);
        if (clear) {
            _changed = false;
            _val = val;
        }
        return ch;
    }

    void markAsChanged() {
        _changed = true;
    }

    T val;

private:
    bool _changed;
    T _val;
};