#include "packet.h"
#include "string.h"
#include <stdexcept>

namespace ryfi {
    Packet::Packet() {}

    Packet::Packet(uint8_t* content, int size) {
        // Check that the size isn't too large
        if (size > MAX_CONTENT_SIZE) {
            throw std::runtime_error("Content size is too large to fit in a packet");
        }


        // Allocate the buffer
        allocate(size);

        // Copy over the content
        memcpy(_content, content, size);
    }

    Packet::Packet(const Packet& b) {
        // Reallocate the buffer
        allocate(b._size);

        // Copy over the content
        memcpy(_content, b._content, b._size);
    }

    Packet::Packet(Packet&& b) {
        // Move members
        _content = b._content;
        _size = b._size;

        // Destroy old object
        b._content = NULL;
        b._size = 0;
    }

    Packet::~Packet() {
        // Delete the content
        if (_content) { delete[] _content; }
    }

    Packet& Packet::operator=(const Packet& b) {
        // Reallocate the buffer
        allocate(b._size);

        // Copy over the content
        memcpy(_content, b._content, b._size);

        // Return self
        return *this;
    }

    Packet& Packet::operator=(Packet&& b) {
        // Move members
        _content = b._content;
        _size = b._size;

        // Destroy old object
        b._content = NULL;
        b._size = 0;

        // Return self
        return *this;
    }

    Packet::operator bool() const {
        return _size > 0;
    }

    int Packet::size() const {
        // Return the size
        return _size;
    }

    const uint8_t* Packet::data() const {
        // Return the size
        return _content;
    }

    void Packet::setContent(uint8_t* content, int size) {
        // Check that the size isn't too large
        if (size > MAX_CONTENT_SIZE) {
            throw std::runtime_error("Content size is too large to fit in a packet");
        }
        
        // Reallocate the buffer
        allocate(size);

        // Copy over the content
        memcpy(_content, content, size);
    }

    int Packet::serializedSize() const {
        // Two size bytes + Size of the content
        return _size + 2;
    }

    int Packet::serialize(uint8_t* bytes) const {
        // Write the size in big-endian
        bytes[0] = (_size >> 8) & 0xFF;
        bytes[1] = _size & 0xFF;

        // Copy the content of the packet
        memcpy(&bytes[2], _content, _size);

        // Return the serialized size
        return serializedSize();
    }

    void Packet::allocate(int newSize) {
        // If the size hasn't changed, do nothing
        if (newSize == _size) { return; }

        // Free the old buffer
        if (_content) { delete[] _content; };

        // Update the size
        _size = newSize;

        // Allocate the buffer
        _content = new uint8_t[newSize];
    }
}