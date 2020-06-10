#pragma once
#include <condition_variable>
#include <algorithm>
#include <math.h>

namespace cdsp {
    template <class T>
    class stream {
    public:
        stream(int size) {
            _buffer = new T[size];
            this->size = size;
            writec = 0;
            readc = size - 1;
            //printf("Stream init\n");
        }

        void read(T* data, int len) {
            int dataRead = 0;
            while (dataRead < len) {
                int canRead = waitUntilReadable();
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
        }

        void readAndSkip(T* data, int len, int skip) {
            int dataRead = 0;
            while (dataRead < len) {
                int canRead = waitUntilReadable();
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
        }

        int waitUntilReadable() {
            int canRead = readable();
            if (canRead > 0) {
                return canRead;
            }
            std::unique_lock<std::mutex> lck(writec_mtx);
            canReadVar.wait(lck, [=](){ return (this->readable(false) > 0); });
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

        void write(T* data, int len) {
            int dataWrite = 0;
            while (dataWrite < len) {
                int canWrite = waitUntilWriteable();
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
        }

        int waitUntilWriteable() {
            int canWrite = writeable();
            if (canWrite > 0) {
                return canWrite;
            }
            std::unique_lock<std::mutex> lck(readc_mtx);
            canWriteVar.wait(lck, [=](){ return (this->writeable(false) > 0); });
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
            return writeable - 1;
        }

    private:
        T* _buffer;
        int size;
        int readc;
        int writec;
        std::mutex readc_mtx;
        std::mutex writec_mtx;
        std::condition_variable canReadVar;
        std::condition_variable canWriteVar;
    };
};