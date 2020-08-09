#pragma once

#include <kaminari/packers/packer.hpp>

#include <unordered_map>


namespace kaminari
{
    struct packet_with_id
    {
        packet_with_id(const packet::ptr& p, entity_id_t i) :
            packet(p),
            id(i)
        {}

        packet::ptr packet;
        entity_id_t id;
    };

    template <class Marshal, class Allocator = std::allocator<detail::pending_data<packet_with_id>>>
    class most_recent_packer_with_id : protected packer<most_recent_packer_with_id<Marshal, Allocator>, packet_with_id, Allocator>
    {
        friend class packer<most_recent_packer_with_id<Marshal, Allocator>, packet_with_id, Allocator>;

    public:
        using packer_t = packer<most_recent_packer_with_id<Marshal, Allocator>, packet_with_id, Allocator>;

    public:
        using packer<most_recent_packer_with_id<Marshal, Allocator>, packet_with_id, Allocator>::packer;
        
        template <typename T, typename... Args>
        void add(uint16_t opcode, T&& data, Args&&... args);
        void process(uint16_t block_id, uint16_t& remaining, detail::packets_by_block& by_block);

    private:
        void add(const packet::ptr& packet, entity_id_t id);

    protected:
        inline void on_ack(const pending_vector_t::iterator& part);
        inline void clear();

    protected:
        std::unordered_map<entity_id_t, typename packer_t::pending_data*> _id_map;
    };


    template <class Marshal, class Allocator>
    template <typename T, typename... Args>
    void most_recent_packer_with_id<Marshal, Allocator>::add(uint16_t opcode, T&& data, Args&&... args)
    {
        // Immediate mode means that the structure is packed right now
        packet::ptr packet = packet::make(opcode, std::forward<Args>(args)...);
        Marshal::pack(packet, data);

        // Add to pending
        add(packet, data.id);
    }

    template <class Marshal, class Allocator>
    void most_recent_packer_with_id<Marshal, Allocator>::add(const packet::ptr& packet, entity_id_t id)
    {
        // Add to pending
        if (auto it = _id_map.find(id); it != _id_map.end())
        {
            auto pending = it->second;
            pending->data.packet = packet;
            pending->blocks.clear();
        }
        else
        {
            // Add to pending
            auto ptr = _allocator.allocate(1);
            auto pending = new (ptr) detail::pending_data<packet_with_id>(packet, id);
            packer_t::_pending.push_back(pending);
            _id_map.emplace(id, pending);
        }
    }

    template <class Marshal, class Allocator>
    inline void most_recent_packer_with_id<Marshal, Allocator>::on_ack(const pending_vector_t::iterator& part)
    {
        // Erased acked entities
        for (auto it = part; it != packer_t::_pending.end(); ++it)
        {
            _id_map.erase((*it)->data.id);
        }
    }

    template <class Marshal, class Allocator>
    void most_recent_packer_with_id<Marshal, Allocator>::process(uint16_t block_id, uint16_t& remaining, detail::packets_by_block& by_block)
    {
        for (auto& pending : _pending)
        {
            if (!is_pending(pending->blocks, block_id, false))
            {
                continue;
            }

            uint16_t actual_block = get_actual_block(pending->blocks, block_id);
            uint16_t size = pending->data.packet->size();
            if (auto it = by_block.find(actual_block); it != by_block.end())
            {
                // TODO(gpascualg): Do we want a hard-break here? packets in the vector should probably be
                //  ordered by size? But we could starve big packets that way, it requires some "agitation"
                //  factor for packets being ignored too much time
                if (size > remaining)
                {
                    break;
                }

                it->second.push_back(pending->data.packet);
            }
            else
            {
                // TODO(gpascualg): Magic numbers, 4 is block header + block size
                // TODO(gpascualg): This can be brought down to 3, block header + packet count
                size += 4;

                // TODO(gpascualg): Same as above, do we want to hard-break?
                if (size > remaining)
                {
                    break;
                }

                by_block.emplace(actual_block, std::initializer_list<packet::ptr> { pending->data.packet });
            }

            pending->blocks.push_back(block_id);
            remaining -= size;
        }
    }

    template <class Marshal, class Allocator>
    inline void most_recent_packer_with_id<Marshal, Allocator>::clear()
    {
        _id_map.clear();
    }
}
