#pragma once

#include <kaminari/cx/overflow.hpp>
#include <kaminari/buffers/packet.hpp>

#include <inttypes.h>
#include <memory.h>
#include <vector>


namespace detail
{
    using packets_by_block = std::map<uint32_t, std::vector<packet::ptr>>;

    template <typename Pending>
    struct pending_data
    {
        pending_data(const Pending& d);

        Pending data;
        std::vector<uint16_t> blocks;
    };
}
    
template <typename Derived, typename Pending, class Allocator = std::allocator<detail::pending_data<Pending>>>
class packer
{
protected:
    using pending_vector_t = std::vector<pending_data<Pending>*>;

public:
    using pending_t = Pending;

public:
    template <typename... AllocatorArgs>
    packer(uint8_t resend_threashold, AllocatorArgs&&... args);

    inline void ack(uint16_t block_id);
    inline void clear();

    bool is_pending(const std::vector<uint16_t>& blocks, uint16_t block_id, bool force);
    inline uint16_t get_actual_block(const std::vector<uint16_t>& blocks, uint16_t block_id);
    inline uint16_t new_block_cost(uint16_t block_id, detail::packets_by_block& by_block);

    uint8_t _resend_threashold;
    pending_vector_t _pending;
    Allocator _allocator;
};


template <typename Pending>
detail::pending_data<Pending>::pending_data(const Pending& d) :
    data(d),
    blocks()
{}

template <typename Derived, typename Pending, class Allocator>
template <typename... AllocatorArgs>
packer<Derived, Pending, Allocator>::packer(uint8_t resend_threashold, AllocatorArgs&&... args) :
    _allocator(std::forward<AllocatorArgs>(args)...),
    _resend_threashold(resend_threashold)
{}

template <typename Derived, typename Pending, class Allocator>
inline void packer<Derived, Pending, Allocator>::ack(uint16_t block_id)
{
    // Partition data
    auto part = std::partition(_pending.begin(), _pending.end(), [block_id](const auto& pending) {
        return std::find(pending->blocks.begin(), pending->blocks.end(), block_id) == pending->blocks.end();
    });

    // Call any callback
    static_cast<Derived&>(*this).on_ack(part);

    // Free memory
    for (auto it = part; it != _pending.end(); ++it)
    {
        std::destroy_at(*it);
        _allocator.deallocate(*it, 1);
    }

    // Erase from vector
    _pending.erase(part, _pending.end());
}

template <typename Derived, typename Pending, class Allocator>
inline void packer<Derived, Pending, Allocator>::clear()
{
    static_cast<Derived&>(*this).clear();

    // Free memory
    for (auto pending : _pending)
    {
        std::destroy_at(*it);
        _allocator.deallocate(*it, 1);
    }

    // Clear
    _pending.clear();
}

template <typename Derived, typename Pending, class Allocator>
bool packer<Derived, Pending, Allocator>::is_pending(const std::vector<uint16_t>& blocks, uint16_t block_id, bool force)
{
    // Do not add the same packet two times, which would probably be due to dependencies
    if (!blocks.empty() && blocks.back() == block_id)
    {
        return false;
    }

    // Pending inclusions are those forced, not yet included in any block or
    // which have expired without an ack
    return force ||
        blocks.empty() ||
        cx::overflow::sub(block_id, blocks.back()) >= _resend_threashold; // We do want 0s here
}

template <typename Derived, typename Pending, class Allocator>
inline uint16_t packer<Derived, Pending, Allocator>::get_actual_block(const std::vector<uint16_t>& blocks, uint16_t block_id)
{
    if (!blocks.empty())
    {
        block_id = blocks.front();
    }
    return block_id;
}

template <typename Derived, typename Pending, class Allocator>
inline uint16_t packer<Derived, Pending, Allocator>::new_block_cost(uint16_t block_id, detail::packets_by_block& by_block)
{
    // TODO(gpascualg): Magic numbers, 4 is block header + block size
    // TODO(gpascualg): This can be brought down to 3, block header + packet count
    // Returns 2 if block_id is not in by_block, otherwise 0
    return static_cast<uint16_t>(by_block.find(block_id) == by_block.end()) * 4;
}
