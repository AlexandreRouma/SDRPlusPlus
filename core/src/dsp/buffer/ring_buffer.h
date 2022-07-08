#pragma once
#include "buffer.h"

#define RING_BUF_SZ 1000000

// IMPORTANT: THIS IS TRASH AND MUST BE REWRITTEN IN THE FUTURE

namespace dsp::buffer {
    template <class T>
    class RingBuffer {
    public:
        RingBuffer() {}

        RingBuffer(int maxLatency) { init(maxLatency); }

        ~RingBuffer() {
            if (!_init) { return; }
            buffer::free(_buffer);
            _init = false;
        }

        void init(int maxLatency) {
            size = RING_BUF_SZ;
            _stopReader = false;
            _stopWriter = false;
            this->maxLatency = maxLatency;
            writec = 0;
            readc = 0;
            readable = 0;
            writable = size;
            _buffer = buffer::alloc<T>(size);
            buffer::clear(_buffer, size);
            _init = true;
        }

        int read(T* data, int len) {
            assert(_init);
            int dataRead = 0;
            int toRead = 0;
            while (dataRead < len) {
                toRead = std::min<int>(waitUntilReadable(), len - dataRead);
                if (toRead < 0) { return -1; };

                if ((toRead + readc) > size) {
                    memcpy(&data[dataRead], &_buffer[readc], (size - readc) * sizeof(T));
                    memcpy(&data[dataRead + (size - readc)], &_buffer[0], (toRead - (size - readc)) * sizeof(T));
                }
                else {
                    memcpy(&data[dataRead], &_buffer[readc], toRead * sizeof(T));
                }

                dataRead += toRead;

                _readable_mtx.lock();
                readable -= toRead;
                _readable_mtx.unlock();
                _writable_mtx.lock();
                writable += toRead;
                _writable_mtx.unlock();
                readc = (readc + toRead) % size;
                canWriteVar.notify_one();
            }
            return len;
        }

        int readAndSkip(T* data, int len, int skip) {
            assert(_init);
            int dataRead = 0;
            int toRead = 0;
            while (dataRead < len) {
                toRead = std::min<int>(waitUntilReadable(), len - dataRead);
                if (toRead < 0) { return -1; };

                if ((toRead + readc) > size) {
                    memcpy(&data[dataRead], &_buffer[readc], (size - readc) * sizeof(T));
                    memcpy(&data[dataRead + (size - readc)], &_buffer[0], (toRead - (size - readc)) * sizeof(T));
                }
                else {
                    memcpy(&data[dataRead], &_buffer[readc], toRead * sizeof(T));
                }

                dataRead += toRead;

                _readable_mtx.lock();
                readable -= toRead;
                _readable_mtx.unlock();
                _writable_mtx.lock();
                writable += toRead;
                _writable_mtx.unlock();
                readc = (readc + toRead) % size;
                canWriteVar.notify_one();
            }
            dataRead = 0;
            while (dataRead < skip) {
                toRead = std::min<int>(waitUntilReadable(), skip - dataRead);
                if (toRead < 0) { return -1; };

                dataRead += toRead;

                _readable_mtx.lock();
                readable -= toRead;
                _readable_mtx.unlock();
                _writable_mtx.lock();
                writable += toRead;
                _writable_mtx.unlock();
                readc = (readc + toRead) % size;
                canWriteVar.notify_one();
            }
            return len;
        }

        int waitUntilReadable() {
            assert(_init);
            if (_stopReader) { return -1; }
            int _r = getReadable();
            if (_r != 0) { return _r; }
            std::unique_lock<std::mutex> lck(_readable_mtx);
            canReadVar.wait(lck, [=]() { return ((this->getReadable(false) > 0) || this->getReadStop()); });
            if (_stopReader) { return -1; }
            return getReadable(false);
        }

        int getReadable(bool lock = true) {
            assert(_init);
            if (lock) { _readable_mtx.lock(); };
            int _r = readable;
            if (lock) { _readable_mtx.unlock(); };
            return _r;
        }

        int write(T* data, int len) {
            assert(_init);
            int dataWritten = 0;
            int toWrite = 0;
            while (dataWritten < len) {
                toWrite = std::min<int>(waitUntilwritable(), len - dataWritten);
                if (toWrite < 0) { return -1; };

                if ((toWrite + writec) > size) {
                    memcpy(&_buffer[writec], &data[dataWritten], (size - writec) * sizeof(T));
                    memcpy(&_buffer[0], &data[dataWritten + (size - writec)], (toWrite - (size - writec)) * sizeof(T));
                }
                else {
                    memcpy(&_buffer[writec], &data[dataWritten], toWrite * sizeof(T));
                }

                dataWritten += toWrite;

                _readable_mtx.lock();
                readable += toWrite;
                _readable_mtx.unlock();
                _writable_mtx.lock();
                writable -= toWrite;
                _writable_mtx.unlock();
                writec = (writec + toWrite) % size;

                canReadVar.notify_one();
            }
            return len;
        }

        int waitUntilwritable() {
            assert(_init);
            if (_stopWriter) { return -1; }
            int _w = getWritable();
            if (_w != 0) { return _w; }
            std::unique_lock<std::mutex> lck(_writable_mtx);
            canWriteVar.wait(lck, [=]() { return ((this->getWritable(false) > 0) || this->getWriteStop()); });
            if (_stopWriter) { return -1; }
            return getWritable(false);
        }

        int getWritable(bool lock = true) {
            assert(_init);
            if (lock) { _writable_mtx.lock(); };
            int _w = writable;
            if (lock) {
                _writable_mtx.unlock();
                _readable_mtx.lock();
            };
            int _r = readable;
            if (lock) { _readable_mtx.unlock(); };
            return std::max<int>(std::min<int>(_w, maxLatency - _r), 0);
        }

        void stopReader() {
            assert(_init);
            _stopReader = true;
            canReadVar.notify_one();
        }

        void stopWriter() {
            assert(_init);
            _stopWriter = true;
            canWriteVar.notify_one();
        }

        bool getReadStop() {
            assert(_init);
            return _stopReader;
        }

        bool getWriteStop() {
            assert(_init);
            return _stopWriter;
        }

        void clearReadStop() {
            assert(_init);
            _stopReader = false;
        }

        void clearWriteStop() {
            assert(_init);
            _stopWriter = false;
        }

        void setMaxLatency(int maxLatency) {
            assert(_init);
            this->maxLatency = maxLatency;
        }

    private:
        bool _init = false;
        T* _buffer;
        int size;
        int readc;
        int writec;
        int readable;
        int writable;
        int maxLatency;
        bool _stopReader;
        bool _stopWriter;
        std::mutex _readable_mtx;
        std::mutex _writable_mtx;
        std::condition_variable canReadVar;
        std::condition_variable canWriteVar;
    };
}