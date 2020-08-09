#pragma once

#include <kaminari/buffers/packet_reader.hpp>
#include <kaminari/buffers/packet.hpp>
#include <kaminari/cx/overflow.hpp>
#include <kaminari/protocol/basic_protocol.hpp>

#include <inttypes.h>


namespace kaminari
{
    class basic_client;
    class protocol;

    class super_packet_reader
    {
    public:
        explicit constexpr super_packet_reader(uint8_t* data);
        super_packet_reader(const super_packet_reader&) = delete; // Do not copy!
        super_packet_reader(super_packet_reader&&) = default;
        ~super_packet_reader() = default;

        inline uint16_t length() const;
        inline uint16_t id() const;

        template <typename C>
        void iterate_acks(C&& callback);

        inline uint8_t* data();
        inline bool has_data();

        template <typename Marshal>
        void handle_packets(basic_client* client, basic_protocol* protocol);
    private:
        uint8_t* _data;
        const uint8_t* _ack_end;
    };


    constexpr super_packet_reader::super_packet_reader(uint8_t* data) :
        _data(data),
        _ack_end(nullptr)
    {}

    inline uint16_t super_packet_reader::length() const
    {
        return *reinterpret_cast<const uint16_t*>(_data);
    }

    inline uint16_t super_packet_reader::id() const
    {
        return *reinterpret_cast<const uint16_t*>(_data + sizeof(uint16_t));
    }

    template <typename C>
    void super_packet_reader::iterate_acks(C&& callback)
    {
        _ack_end = _data + sizeof(uint16_t) * 2;
        uint8_t num_acks = *reinterpret_cast<const uint8_t*>(_ack_end);
        _ack_end += sizeof(uint8_t);

        for (uint8_t i = 0; i < num_acks; ++i)
        {
            uint16_t ack = *reinterpret_cast<const uint16_t*>(_ack_end);
            callback(ack);
            _ack_end += sizeof(uint16_t);
        }
    }

    inline uint8_t* super_packet_reader::data()
    {
        return _data;
    }

    inline bool super_packet_reader::has_data()
    {
        return *reinterpret_cast<const uint8_t*>(_ack_end) != 0x0;
    }

    template <typename Marshal>
    void super_packet_reader::handle_packets(basic_client* client, basic_protocol* protocol)
    {
        // Start reading old blocks
        uint8_t num_blocks = *reinterpret_cast<const uint8_t*>(_ack_end);
        const uint8_t* block_pos = _ack_end + sizeof(uint8_t);

        // Set some upper limit to avoid exploits
        int remaining = 500 - (block_pos - _data); // Keep it signed on purpouse
        for (uint8_t i = 0; i < num_blocks; ++i)
        {
            uint16_t block_id = *reinterpret_cast<const uint16_t*>(block_pos);
            uint8_t num_packets = *reinterpret_cast<const uint8_t*>(block_pos + sizeof(uint16_t));
            if (num_packets == 0)
            {
                // TODO(gpascualg): Should we kick the player for packet forging?
                return;
            }

            const uint64_t block_timestamp = protocol->block_timestamp(block_id);
            block_pos += sizeof(uint16_t) + sizeof(uint8_t);
            remaining -= sizeof(uint16_t) + sizeof(uint8_t);

            for (uint8_t j = 0; j < num_packets && remaining > 0; ++j)
            {
                packet_reader reader(block_pos, block_timestamp, remaining);
                uint16_t length = reader.length();
                block_pos += length;
                remaining -= length;

                if (length < packet::DataStart || remaining < 0)
                {
                    // TODO(gpascualg): Should we kick the player for packet forging?
                    return;
                }   

                if (protocol->resolve(&reader, block_id))
                {
                    Marshal::handle_packet(&reader, client);
                }
            }
        }
    }
}
