#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>

const int default_count = 64;
const int spin_count = 100;
#define ALIGN (8)

class ringbufferbase {
public:
    ringbufferbase(int count) :
        max_count(count),
        read_index(0),
        write_index(0),
        emptyCount(0),
        fullCount(0),
        writeCount(0)
    {
    }

    int getFullCount() const { return fullCount; }

    int getEmptyCount() const { return emptyCount; }

    int getWriteCount() const { return writeCount; }

    void ReadDone()
    {
        std::unique_lock<std::mutex> lk(mutex);
        if ((write_index + 1) % max_count == read_index)
        {
            read_index = (read_index + 1) % max_count;
            nonfullCV.notify_all();
        }
        else
        {
            read_index = (read_index + 1) % max_count;
        }
    }

    void WriteDone()
    {
        std::unique_lock<std::mutex> lk(mutex);
        if (read_index == write_index)
        {
            write_index = (write_index + 1) % max_count;
            nonemptyCV.notify_all();
        }
        else
        {
            write_index = (write_index + 1) % max_count;
        }
        writeCount++;
    }

    void Stop()
    {
        std::unique_lock<std::mutex> lk(mutex);
        read_index = 0;
        write_index = max_count / 2;
        nonfullCV.notify_all();
        nonemptyCV.notify_all();
    }

protected:

    void WaitUntilNotEmpty()
    {
        // if not empty
        for (int i = 0; i < spin_count; i++)
        {
            if (read_index != write_index)
                return;
        }

        if (read_index == write_index)
        {
            std::unique_lock<std::mutex> lk(mutex);

            emptyCount++;
            nonemptyCV.wait(lk, [this] {
                return read_index != write_index;
            });
        }
    }

    void WaitUntilNotFull()
    {
        for (int i = 0; i < spin_count; i++)
        {
            if ((write_index + 1) % max_count != read_index)
                return;
        }

        if ((write_index + 1) % max_count == read_index)
        {
            std::unique_lock<std::mutex> lk(mutex);
            fullCount++;
            nonfullCV.wait(lk, [this] {
                return (write_index + 1) % max_count != read_index;
            });
        }
    }

    int max_count;

    volatile int read_index;
    volatile int write_index;

private:
    int emptyCount;
    int fullCount;
    int writeCount;

    std::mutex mutex;
    std::condition_variable nonemptyCV;
    std::condition_variable nonfullCV;
};

template<typename T> class ringbuffer : public ringbufferbase {
    typedef T* TPtr;

public:
    ringbuffer(int count = default_count) :
        ringbufferbase(count)
    {
        buffers = new TPtr[max_count];
        buffers[0] = nullptr;
    }

    ~ringbuffer()
    {
        if (buffers[0])
            delete[] buffers[0];

        delete[] buffers;
    }

    void setBlockSize(int size)
    {
        if (block_size != size)
        {
            block_size = size;

            if (buffers[0])
                delete[] buffers[0];

            int aligned_block_size = (block_size + ALIGN - 1) & (~(ALIGN - 1));

            auto data = new T[max_count * aligned_block_size];

            for (int i = 0; i < max_count; ++i)
            {
                buffers[i] = &data[i * aligned_block_size];
            }
        }
    }

    T* peekWritePtr(int offset)
    {
        return buffers[(write_index + max_count + offset) % max_count];
    }

    T* peekReadPtr(int offset)
    {
        return buffers[(read_index + max_count + offset) % max_count];
    }

    T* getWritePtr()
    {
        // if there is still space
        WaitUntilNotFull();
        return buffers[(write_index) % max_count];
    }

    const T* getReadPtr()
    {
        WaitUntilNotEmpty();

        return buffers[read_index];
    }

    int getBlockSize() const { return block_size; }

private:
    int block_size;

    TPtr* buffers;
};