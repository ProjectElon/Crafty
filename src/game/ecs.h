#pragma once

#include "core/common.h"
#include "game/components.h"

#include <tuple>
#include <bitset>

namespace minecraft {

    template< typename T >
    u32 get_component_id()
    {
        static u32 id = ECS::internal_data.component_id_counter++;
        return id;
    }

    // todo(harlequin): we can calculate "max_component_count" using mata programming
    const u32 max_component_count = 32;
    typedef std::bitset< max_component_count > Component_Mask;

    struct Component_Pool
    {
        u8 *base;
        size_t component_size;

        // todo(harlequin): alignment requirements
        void allocate(size_t component_size, u32 max_entity_count);
        void free();

        inline u8* get(u32 component_index)
        {
            return this->base + component_index * this->component_size;
        }
    };

    enum EntityTag : u16
    {
        EntityTag_None,
        EntityTag_Player,
        EntityTag_Camera
    };

    enum EntityArchetype : u16
    {
        EntityArchetype_None,
        EntityArchetype_Guy
    };

    typedef u64 Entity;

    #define make_entity(index, generation) (((u64)((index) << 32)) | (u64)(generation))
    #define get_entity_index(entity) (u32)((entity) >> 32)
    #define get_entity_generation(entity) (u32)(entity)

    struct Entity_Info
    {
        EntityTag tag;
        EntityArchetype archetype;
        u32 generation;
        Component_Mask mask;
    };

    struct Registry
    {
        u32 max_entity_count;
        std::vector< u32 > free_entities;
        std::vector< Entity_Info > entities;
        std::vector< Component_Pool > component_pools;
        std::unordered_map< u16, Entity > tagged_entities;

        void initialize(const u32 max_entity_count = 1024);

        Entity create_entity(EntityArchetype archetype = EntityArchetype_None,
                             EntityTag tag = EntityTag_None);
        void destroy_entity(Entity entity);

        inline bool is_entity_valid(Entity entity)
        {
            u32 index = get_entity_index(entity);
            if (index < 0 || index >= max_entity_count) return false;
            u32 generation = get_entity_generation(entity);
            Entity_Info& info = this->entities[index];
            return info.generation == generation;
        }

        template< typename T >
        T* add_component(Entity entity)
        {
            u32 component_id = get_component_id< T >();
            assert(component_id < max_component_count);

            auto& pool = component_pools[component_id];

            if (!pool.base)
            {
                pool.allocate(sizeof(T), max_entity_count);
            }

            T* component = (T*)pool.get(entity);
            memset(component, 0, sizeof(T));

            Entity_Info& info = this->entities[get_entity_index(entity)];
            info.mask.set(component_id);

            return component;
        }

        template< typename T >
        T* get_component(Entity entity)
        {
            u32 component_id = get_component_id< T >();
            Entity_Info& info = entities[get_entity_index(entity)];
            if (!info.mask[component_id])
            {
                return nullptr;
            }
            auto& pool = component_pools[component_id];
            T* component = (T*)pool.get(entity);
            return component;
        }

        template< typename ...Components >
        auto get_components(Entity entity)
        {
            return std::make_tuple(get_component<Components>(entity)...);
        }

        template< typename T >
        void remove_component(Entity entity)
        {
            u32 component_id = get_component_id< T >();
            u32 index = get_entity_index(entity);
            Entity_Info& info = entities[index];
            info.mask.reset(component_id);
        }

        Entity find_entity_by_tag(EntityTag tag);
    };

    struct Registry_View
    {
        Registry *registry;
        Component_Mask mask;

        inline bool is_valid(u32 entity_index) const
        {
            return (this->registry->entities[entity_index].mask & this->mask) == this->mask;
        }

        inline Entity begin()
        {
            u32 current_entity_index = 0;

            while (current_entity_index < this->registry->entities.size() && !is_valid(current_entity_index))
            {
                ++current_entity_index;
            }

            return current_entity_index == this->registry->entities.size() ? end() : make_entity(current_entity_index, this->registry->entities[current_entity_index].generation);
        }

        inline Entity end()
        {
            return make_entity(this->registry->entities.size(), 0);
        }

        inline Entity next(Entity entity)
        {
            u32 current_entity_index = get_entity_index(entity);

            do
            {
                ++current_entity_index;
            }
            while (current_entity_index < this->registry->entities.size() && !is_valid(current_entity_index));

            return current_entity_index == this->registry->entities.size() ? end() : make_entity(current_entity_index, this->registry->entities[current_entity_index].generation);
        }
    };

    template< typename ...Components >
    Registry_View get_view(Registry *registry)
    {
        Registry_View view;
        view.registry = registry;

        if (sizeof...(Components) != 0)
        {
            u32 component_ids[] = { get_component_id< Components >()... };

            for (u32 i = 0; i < sizeof...(Components); i++)
            {
                view.mask.set(component_ids[i]);
            }
        }

        return view;
    }

    struct ECS_Data
    {
        u32 component_id_counter = 0;
        Registry registry;
    };

    struct ECS
    {
        static ECS_Data internal_data;

        static bool initialize(u32 max_entity_count);
        static void shutdown();
    };
}
