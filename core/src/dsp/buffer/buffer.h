#pragma once
#include <volk/volk.h>

namespace dsp::buffer {
    template<class T>
    inline T* alloc(int count) {
        return (T*)volk_malloc(count * sizeof(T), volk_get_alignment());
    }

    template<class T>
    inline void clear(T* buffer, int count, int offset = 0) {
        memset(&buffer[offset], 0, count * sizeof(T));
    }

    inline void free(void* buffer) {
        volk_free(buffer);
    }
}