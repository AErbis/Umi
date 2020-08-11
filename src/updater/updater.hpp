#pragma once

#include "fiber/exclusive_work_stealing.hpp"

#include <boost/fiber/fiber.hpp>
#include <boost/fiber/mutex.hpp>
#include <boost/fiber/condition_variable.hpp>

#include <variant>
#include <vector>


template <typename... types>
class updater
{
public:
    updater(bool contiguous_component_execution) :
        _contiguous_component_execution(contiguous_component_execution),
        _pending_updates(0)
    {}

    template <typename... vectors>
    updater(bool contiguous_component_execution, std::tuple<vectors...>& components) :
        updater{ contiguous_component_execution }
    {
        std::apply([this](auto&... comps) {
            (register_vector(&comps), ...);
        }, components);
    }

    updater(const updater&) = delete;
    updater(updater&&) = default;

    template <typename... Args>
    void update(Args&&... args)
    {
        _pending_updates = 0;

        for (auto& variant : _vectors)
        {
            std::visit([this, ...args { std::forward<Args>(args) }](auto& vec) mutable {
                using E = typename std::remove_pointer<std::decay_t<decltype(vec)>>::type;

                if (!E::derived_t::template has_update<std::decay_t<Args>...>())
                {
                    return;
                }
                else
                {
                    _updates_mutex.lock();
                    _pending_updates += vec->size();
                    _updates_mutex.unlock();

                    boost::fibers::fiber([this, &vec, ...args{ std::forward<Args>(args) }]() mutable {
                        update_impl(vec, std::forward<std::decay_t<Args>>(args)...);
                    }).detach();
                }
                
            }, variant);
        }
    }

    void wait_update()
    {
        _updates_mutex.lock();
        _updates_cv.wait(_updates_mutex, [this]() { return _pending_updates == 0; });
        _updates_mutex.unlock();
    }

    template <typename... Args>
    void sync(Args&&... args)
    {
        for (auto& variant : _vectors)
        {
            std::visit([this, ...args { std::forward<Args>(args) }](auto& vec) mutable {
                using E = typename std::remove_pointer<std::decay_t<decltype(vec)>>::type;

                if constexpr (!E::derived_t::template has_sync<Args...>())
                {
                    return;
                }
                else
                {
                    for (auto obj : vec->range())
                    {
                        obj->base()->sync(std::forward<Args>(args)...);
                    }
                }
            }, variant);
        }
    }

    template <typename T>
    void register_vector(T* vector)
    {
        _vectors.emplace_back(vector);
    }

    template <typename T>
    bool unregister_vector(T* vector)
    {
        for (auto it = _vectors.begin(); it != _vectors.end(); ++it)
        {
            auto& variant = *it;

            if (std::holds_alternative<T*>(variant))
            {
                auto& vec = std::get<T*>(variant);
                if (vec == vector)
                {
                    _vectors.erase(it);
                    return true;
                }
            }
        }

        return false;
    }

private:
    template <typename T, typename... Args>
    void update_impl(T* vector, Args&&... args)
    {
        if (_contiguous_component_execution)
        {
            reinterpret_cast<exclusive_work_stealing<0>*>(get_scheduling_algorithm().get())->start_bundle();
        }

        for (auto obj : vector->range())
        {

            boost::fibers::fiber([this, obj, ...args{ std::forward<Args>(args) }]() mutable {
                obj->base()->update(std::forward<Args>(args)...);
                
                _updates_mutex.lock();
                --_pending_updates;
                _updates_mutex.unlock();

                if (_pending_updates == 0)
                {
                    _updates_cv.notify_all();
                }
            }).detach();
        }

        if (_contiguous_component_execution)
        {
            reinterpret_cast<exclusive_work_stealing<0>*>(get_scheduling_algorithm().get())->end_bundle();
        }
    }

private:
    bool _contiguous_component_execution;
    std::vector<std::variant<types...>> _vectors;
    uint64_t _pending_updates;
    boost::fibers::mutex _updates_mutex;
    boost::fibers::condition_variable_any _updates_cv;
};
