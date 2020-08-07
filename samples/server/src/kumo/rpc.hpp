#pragma once
#include <kumo/opcodes.hpp>
#include <kumo/rpc_detail.hpp>
#include <kumo/structs.hpp>
#include <kaminari/buffers/packet.hpp>
#include <kaminari/broadcaster.hpp>
class client;
namespace kumo
{
    inline void send_do_sth(::kumo::protocol_queues* pq, complex&& data, T&& callback);
    template <typename B, typename T>
    void broadcast_do_sth(::kaminari::broadcaster<B>* broadcaster, complex&& data, T&& callback);
    template <typename B, typename T>
    void broadcast_do_sth_single(::kaminari::broadcaster<B>* broadcaster, complex&& data, T&& callback);
    inline void send_spawn(::kumo::protocol_queues* pq, spawn_data&& data, T&& callback);
    template <typename B, typename T>
    void broadcast_spawn(::kaminari::broadcaster<B>* broadcaster, spawn_data&& data, T&& callback);
    template <typename B, typename T>
    void broadcast_spawn_single(::kaminari::broadcaster<B>* broadcaster, spawn_data&& data, T&& callback);
}

namespace kumo
{
    inline void send_do_sth(::kumo::protocol_queues* pq, complex&& data, T&& callback)
    {
        pq->send_reliable(opcode::do_sth, std::move(data), std::forward<T>(callback));
    }
    template <typename B, typename T>
    void broadcast_do_sth(::kaminari::broadcaster<B>* broadcaster, complex&& data, T&& callback)
    {
        boost::intrusive_ptr<::kaminari::packet> packet = ::kaminari::packet::make(opcode::do_sth, std::forward<T>(callback));
        ::kumo::pack(packet, data);
        broadcaster->broadcast([packet](auto pq) {
            pq->send_reliable(packet);
        });
    }
    template <typename B, typename T>
    void broadcast_do_sth_single(::kaminari::broadcaster<B>* broadcaster, complex&& data, T&& callback)
    {
        boost::intrusive_ptr<::kaminari::packet> packet = ::kaminari::packet::make(opcode::do_sth, std::forward<T>(callback));
        ::kumo::pack(packet, data);
        broadcaster->broadcast([packet](auto pq) {
            pq->send_reliable(packet);
        });
    }
    inline void send_spawn(::kumo::protocol_queues* pq, spawn_data&& data, T&& callback)
    {
        pq->send_reliable(opcode::spawn, std::move(data), std::forward<T>(callback));
    }
    template <typename B, typename T>
    void broadcast_spawn(::kaminari::broadcaster<B>* broadcaster, spawn_data&& data, T&& callback)
    {
        boost::intrusive_ptr<::kaminari::packet> packet = ::kaminari::packet::make(opcode::spawn, std::forward<T>(callback));
        ::kumo::pack(packet, data);
        broadcaster->broadcast([packet](auto pq) {
            pq->send_reliable(packet);
        });
    }
    template <typename B, typename T>
    void broadcast_spawn_single(::kaminari::broadcaster<B>* broadcaster, spawn_data&& data, T&& callback)
    {
        boost::intrusive_ptr<::kaminari::packet> packet = ::kaminari::packet::make(opcode::spawn, std::forward<T>(callback));
        ::kumo::pack(packet, data);
        broadcaster->broadcast([packet](auto pq) {
            pq->send_reliable(packet);
        });
    }
}
