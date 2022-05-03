#include "ecs.h"

namespace minecraft {

    bool ECS::initialize(u32 max_entity_count)
    {
        internal_data.registry.initialize(max_entity_count);
        return true;
    }

    void ECS::shutdown()
    {
    }

    void Component_Pool::allocate(size_t component_size, u32 max_entity_count)
    {
        this->component_size = component_size;
        this->base = new u8[component_size * max_entity_count];
    }

    void Component_Pool::free()
    {
        delete[] this->base;
    }

    void Registry::initialize(const u32 max_entity_count)
    {
        this->max_entity_count = max_entity_count;

        this->component_pools.resize(max_component_count);
        this->entities.resize(max_entity_count);
        this->free_entities.resize(max_entity_count);

        for (u32 i = 0; i < max_component_count; i++)
        {
            auto& component_pool = this->component_pools[i];
            component_pool.base = nullptr;
            component_pool.component_size = 0;
        }

        for (u32 i = 0; i < max_entity_count; i++)
        {
            Entity_Info& info = this->entities[i];

            info.tag = EntityTag_None;
            info.archetype = EntityArchetype_None;
            info.generation = 1;
            info.mask.reset();

            this->free_entities[i] = max_entity_count - i - 1;
        }
    }

    Entity Registry::create_entity(EntityArchetype archetype,
                                   EntityTag tag)
    {
        assert(this->free_entities.size());

        u32 entity_index = this->free_entities.back();
        free_entities.pop_back();
        Entity_Info& info = this->entities[entity_index];
        Entity entity = make_entity(entity_index, info.generation);

        info.tag = tag;
        info.archetype = archetype;

        if (tag != EntityTag_None)
        {
            this->tagged_entities.emplace(tag, entity);
        }

        return entity;
    }

    void Registry::destroy_entity(Entity entity)
    {
        assert(is_entity_valid(entity));

        Entity_Info& info = this->entities[entity];
        if (info.tag != EntityTag_None)
        {
            auto it = this->tagged_entities.find(info.tag);
            this->tagged_entities.erase(it);
        }
        info.tag = EntityTag_None;
        info.archetype = EntityArchetype_None;
        info.generation++;
        info.mask.reset();

        u32 index = get_entity_index(entity);
        free_entities.emplace_back(index);
    }

    Entity Registry::find_entity_by_tag(EntityTag tag)
    {
        auto it = this->tagged_entities.find(tag);
        if (it == this->tagged_entities.end())
        {
            return make_entity(this->entities.size(), 0);
        }
        u32 entity_index = it->second;
        Entity_Info& info = this->entities[entity_index];
        return make_entity(entity_index, info.generation);
    }

    ECS_Data ECS::internal_data;
}