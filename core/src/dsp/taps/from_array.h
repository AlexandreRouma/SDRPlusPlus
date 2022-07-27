#pragma once
#include "tap.h"
#include "../math/sinc.h"
#include "../math/hz_to_rads.h"
#include "../window/nuttall.h"

namespace dsp::taps {
    template<class T>
    inline tap<T> fromArray(int count, const T* taps) {
        // Allocate taps
        tap<T> _taps = taps::alloc<T>(count);
        
        // Copy data
        memcpy(_taps.taps, taps, count * sizeof(T));

        return _taps;
    }
}