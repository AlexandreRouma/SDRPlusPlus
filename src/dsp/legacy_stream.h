#pragma once
#include <condition_variable>
#include <algorithm>
#include <math.h>

#define STREAM_BUF_SZ   1000000

namespace dsp {
    template <class T>
    class stream {
    public:
        stream() {

        }

        stream(int maxLatency) {
            size = STREAM_BUF_SZ;
            _buffer = new T[size];
            _stopReader = false;
            _stopWriter = false;
            this->maxLatency = maxLatency;
            writec = 0;
            readc = size - 1;
        }

        void init(int maxLatency) {
            size = STREAM_BUF_SZ;
            _buffer = new T[size];
            _stopReader = false;
            _stopWriter = false;
            this->maxLatency = maxLatency;
            writec = 0;
            readc = size - 1;
        }

        int read(T* data, int len) {
            int dataRead = 0;
            while (dataRead < len) {
                int canRead = waitUntilReadable();
                if (canRead < 0) {
                    printf("Reader stopped\n");
                    clearReadStop();
                    return -1;
                }
                int toRead = std::min(canRead, len - dataRead);

                int len1 = (toRead >= (size - readc) ? (size - readc) : (toRead));

                memcpy(&data[dataRead], &_buffer[readc], len1 * sizeof(T));
                if (len1 < toRead) {
                    memcpy(&data[dataRead + len1], _buffer, (toRead - len1) * sizeof(T));
                }

                dataRead += toRead;
                readc_mtx.lock();
                readc = (readc + toRead) % size;
                readc_mtx.unlock();
                canWriteVar.notify_one();
            }
            return len;
        }

        int readAndSkip(T* data, int len, int skip) {
            int dataRead = 0;
            while (dataRead < len) {
                int canRead = waitUntilReadable();
                if (canRead < 0) {
                    printf("reader stopped (read and skip)\n");
                    clearReadStop();
                    return -1;
                }
                int toRead = std::min(canRead, len - dataRead);

                int len1 = (toRead >= (size - readc) ? (size - readc) : (toRead));
                memcpy(&data[dataRead], &_buffer[readc], len1 * sizeof(T));
                if (len1 < toRead) {
                    memcpy(&data[dataRead + len1], _buffer, (toRead - len1) * sizeof(T));
                }

                dataRead += toRead;
                readc_mtx.lock();
                readc = (readc + toRead) % size;
                readc_mtx.unlock();
                canWriteVar.notify_one();
            }

            // Skip

            dataRead = 0;
            while (dataRead < skip) {
                int canRead = waitUntilReadable();
                int toRead = std::min(canRead, skip - dataRead);

                dataRead += toRead;
                readc_mtx.lock();
                readc = (readc + toRead) % size;
                readc_mtx.unlock();
                canWriteVar.notify_one();
            }
            return len;
        }

        int waitUntilReadable() {
            int canRead = readable();
            if (canRead > 0) {
                return canRead;
            }
            std::unique_lock<std::mutex> lck(writec_mtx);
            canReadVar.wait(lck, [=](){ return ((this->readable(false) > 0) || this->getReadStop()); });
            if (this->getReadStop()) {
                return -1;
            }
            return this->readable(false);
        }

        int readable(bool lock = true) {
            if (lock) { writec_mtx.lock(); }
            int _wc = writec;
            if (lock) { writec_mtx.unlock(); }
            int readable = (_wc - readc) % this->size;
            if (_wc < readc) {
                readable = (this->size + readable);
            }
            return readable - 1;
        }

        int write(T* data, int len) {
            int dataWrite = 0;
            while (dataWrite < len) {
                int canWrite = waitUntilWriteable();
                if (canWrite < 0) {
                    printf("Writer stopped\n");
                    clearWriteStop();
                    return -1;
                }
                int toWrite = std::min(canWrite, len - dataWrite);

                int len1 = (toWrite >= (size - writec) ? (size - writec) : (toWrite));

                memcpy(&_buffer[writec], &data[dataWrite], len1 * sizeof(T));
                if (len1 < toWrite) {
                    memcpy(_buffer, &data[dataWrite + len1], (toWrite - len1) * sizeof(T));
                }

                dataWrite += toWrite;
                writec_mtx.lock();
                writec = (writec + toWrite) % size;
                writec_mtx.unlock();
                canReadVar.notify_one();
            }
            return len;
        }

        int waitUntilWriteable() {
            int canWrite = writeable();
            if (canWrite > 0) {
                return canWrite;
            }
            std::unique_lock<std::mutex> lck(readc_mtx);
            canWriteVar.wait(lck, [=](){ return ((this->writeable(false) > 0) || this->getWriteStop()); });
            if (this->getWriteStop()) {
                return -1;
            }
            return this->writeable(false);
        }

        int writeable(bool lock = true) {
            if (lock) { readc_mtx.lock(); }
            int _rc = readc;
            if (lock) { readc_mtx.unlock(); }
            int writeable = (_rc - writec) % this->size;
            if (_rc < writec) {
                writeable = (this->size + writeable);
            }
            return std::min<float>(writeable - 1, maxLatency - readable(false) - 1);
        }

        void stopReader() {
            _stopReader = true;
            canReadVar.notify_one();
        }

        void stopWriter() {
            _stopWriter = true;
            canWriteVar.notify_one();
        }

        bool getReadStop() {
            return _stopReader;
        }

        bool getWriteStop() {
            return _stopWriter;
        }

        void clearReadStop() {
            _stopReader = false;
        }

        void clearWriteStop() {
            _stopWriter = false;
        }

        void setMaxLatency(int maxLatency) {
            this->maxLatency = maxLatency;
        }

    private:
        T* _buffer;
        int size;
        int readc;
        int writec;
        int maxLatency;
        bool _stopReader;
        bool _stopWriter;
        std::mutex readc_mtx;
        std::mutex writec_mtx;
        std::condition_variable canReadVar;
        std::condition_variable canWriteVar;
    };
};