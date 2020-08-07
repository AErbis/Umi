#pragma once
#include <inttypes.h>
#include <boost/intrusive_ptr.hpp>
#include <kumo/opcodes.hpp>
namespace kaminari
{
    class packet;
}
namespace kaminari
{
    namespace detail
    {
        using packets_by_block = std::map<uint32_t, std::vector<Packet::Ptr>>;
    }
}
namespace kumo
{
    class protocol_queues;
}

namespace kumo
{
    class protocol_queues
    {
    private:
        void reset();
        void ack(uint16_t block_id);
        void process(uint16_t id, uint16_t& remaining, typename ::kumo::detail::packets_by_block& by_block);
        template <typename D, typename T>
        void send_reliable(::kumo::opcode opcode, D&& data, T&& callback);
        void send_reliable(const boost::intrusive_ptr<::kaminari::packet>& packet);
        template <typename D, typename T>
        void send_ordered(::kumo::opcode opcode, D&& data, T&& callback);
        void send_ordered(const boost::intrusive_ptr<::kaminari::packet>& packet);
    private:
        ::kaminari::reliable_queue<::kaminari::immediate_packer> _reliable;
        ::kaminari::reliable_queue<::kaminari::ordered_packer> _ordered;
    };

    template <typename D, typename T>
    void protocol_queues::send_reliable(::kumo::opcode opcode, D&& data, T&& callback)
    {
        _reliable.add(opcode, std::forward<D>(data), std::forward<T>(callback));
    }
    template <typename D, typename T>
    void protocol_queues::send_ordered(::kumo::opcode opcode, D&& data, T&& callback)
    {
        _ordered.add(opcode, std::forward<D>(data), std::forward<T>(callback));
    }
}
