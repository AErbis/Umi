#pragma once

#include <containers/ticket.hpp>
#include <entity/entity.hpp>
#include <entity/scheme.hpp>

#include "maps/offset.hpp"
#include "entities/map_aware.hpp"
#include "entities/transform.hpp"


class transform;
class map;


class region
{
    friend class map;

public:
    constexpr static inline uint16_t static_alloc_size = 150;
    using offset_t = offset<float, 150>;
    template <typename T> using dic_t = dictionary<T, entity<T>, static_alloc_size>;

    region(map* map, const offset_t& offset);

    // TODO(gpascualg): I don't like this name (can use map, but then I'd have to class map everything else)
    inline map* get_map() const;
    inline const offset_t& offset() const;
    inline updater_contiguous<region::dic_t<transform>*>& updater();

    template <typename C>
    void create_entity(map* map, cell* cell, uint64_t id, uint64_t db_id, const glm::vec3& position, C&& callback);
    void remove_entity(transform* transform);
    void move_to(region* other, map_aware* who, transform* trf);

private:
    void on_entity_created(map_aware* map_aware, transform* transform, const glm::vec3& position);

private:
    map* _map;
    offset_t _offset;

    scheme_store<dic_t<map_aware>, dic_t<transform>> _common_store;
    scheme_store<dic_t<map_aware>, dic_t<transform>> _moving_store;

    decltype(scheme_maker<map_aware, transform>()(_common_store)) _map_aware_scheme;
    decltype(scheme_maker<map_aware, transform>()(_moving_store)) _moving_transforms_scheme;

    updater_contiguous<region::dic_t<transform>*> _transforms_updater;
};


inline map* region::get_map() const
{
    return _map;
}

inline const region::offset_t& region::offset() const
{
    return _offset;
}

inline updater_contiguous<region::dic_t<transform>*>& region::updater()
{
    return _transforms_updater;
}

template <typename C>
void region::create_entity(map* map, cell* cell, uint64_t id, uint64_t db_id, const glm::vec3& position, C&& callback)
{
    server::instance->create_with_callback(
        id,
        _map_aware_scheme,
        [this, position, callback{ std::move(callback) }](map_aware* map_aware, transform* transform) mutable {
            on_entity_created(map_aware, transform, position);
            callback(map_aware, transform);
            return tao::tuple(map_aware, transform);
        },
        _map_aware_scheme.args<map_aware>(db_id),
        _map_aware_scheme.args<transform>(map, this, cell)
    );
}
