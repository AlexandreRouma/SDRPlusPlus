#pragma once
#include <stdint.h>

namespace ryfi {
    /**
     * RyFi Protocol Packet.
    */
    class Packet {
    public:
        // Default constructor
        Packet();

        /**
         * Create a packet from its content.
         * @param content Content of the packet.
         * @param size Number of bytes of content.
        */
        Packet(uint8_t* content, int size);

        // Copy constructor
        Packet(const Packet& b);

        // Move constructor
        Packet(Packet&& b);

        // Destructor
        ~Packet();

        // Copy assignment operator
        Packet& operator=(const Packet& b);

        // Move assignment operator
        Packet& operator=(Packet&& b);

        // Cast to bool operator
        operator bool() const;

        /**
         * Get the size of the content of the packet.
         * @return Size of the content of the packet.
        */
        int size() const;

        /**
         * Get the content of the packet. The pointer is only valid until reallocation or deletion.
         * @return Content of the packet.
        */
        const uint8_t* data() const;

        /**
         * Set the content of the packet.
         * @param content Content of the packet.
         * @param size Number of bytes of content.
        */
        void setContent(uint8_t* content, int size);

        /**
         * Get the size of the serialized packet.
         * @return Size of the serialized packet.
        */
        int serializedSize() const;

        /**
         * Serialize the packet to bytes.
         * @param bytes Buffer to which to write the serialized packet.
         * @return Size of the serialized packet.
        */
        int serialize(uint8_t* bytes) const;

        /**
         * Deserialize a packet from bytes.
         * TODO
        */
        static bool deserialize(uint8_t* bytes, int size, Packet& pkt);

        // Maximum size of the content of the packet.
        static inline const int MAX_CONTENT_SIZE    = 0xFFFF;

        // Maximum size of the serialized packet.
        static inline const int MAX_SERIALIZED_SIZE = MAX_CONTENT_SIZE + 2;

    private:
        void allocate(int newSize);

        uint8_t* _content = NULL;
        int _size = 0;
    };
}