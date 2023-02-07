#pragma once

#include "core/common.h"

namespace minecraft {

    enum HashTableEntryState : u8
    {
        HashTableEntryState_Empty    = 0x0,
        HashTableEntryState_Deleted  = 0x1,
        HashTableEntryState_Occupied = 0x2
    };

    template< typename Key, typename Value, const u32 MaxCount >
    struct Hash_Table
    {
        u32                 count;
        HashTableEntryState entry_states[MaxCount];
        Value               entries[MaxCount];
        Key                 keys[MaxCount];

        void initialize()
        {
            count = 0;
            memset(entry_states, 0, sizeof(HashTableEntryState) * MaxCount);
        }

        struct Iterator
        {
            Value *entry;
        };

        // struct delete_result
        // {
        //     bool  Deleted;
        //     Value Entry;
        // };

        Iterator insert(const Key &key, const Value &value)
        {
            u32 start_index     = hash(key) % MaxCount;
            u32 index           = start_index;
            u32 insertion_index = -1;
            Iterator iterator   = {};

            do
            {
                HashTableEntryState &entry_state = entry_states[index];

                if (entry_state == HashTableEntryState_Empty)
                {
                    bool IsInsertionIndexSet = insertion_index != -1;
                    if (!IsInsertionIndexSet)
                    {
                        insertion_index = index;
                    }
                    break;
                }
                else if (entry_state == HashTableEntryState_Deleted)
                {
                    bool IsInsertionIndexSet = insertion_index != -1;
                    if (!IsInsertionIndexSet)
                    {
                        insertion_index = index;
                    }
                }
                else if (keys[index] == key)
                {
                    entries[index] = value;
                    iterator.entry = &entries[index];
                    return iterator;
                }

                index++;

                if (index == MaxCount)
                {
                    index = 0;
                }
            }
            while (index != start_index);

            count++;
            entry_states[insertion_index] = HashTableEntryState_Occupied;
            keys[insertion_index]         = key;
            entries[insertion_index]      = value;
            iterator.entry = &entries[insertion_index];

            return iterator;
        }

        Iterator find(const Key &key)
        {
            u32 start_index    = hash(key) % MaxCount;
            u32 index          = start_index;
            Iterator iterator  = {};

            do
            {
                HashTableEntryState &entry_state = entry_states[index];

                if (entry_state == HashTableEntryState_Empty)
                {
                    break;
                }
                else if (entry_state == HashTableEntryState_Occupied &&
                         keys[index] == key)
                {

                    iterator.entry = &entries[index];
                    break;
                }

                index++;

                if (index == MaxCount)
                {
                    index = 0;
                }
            }
            while (index != start_index);

            return iterator;
        }

        Value remove(const Key &key)
        {
            u32 start_index = hash(key) % MaxCount;
            u32 index       = start_index;

            do
            {
                HashTableEntryState &entry_state = entry_states[index];
                if (entry_state == HashTableEntryState_Empty)
                {
                    break;
                }
                else if (entry_state == HashTableEntryState_Occupied &&
                         keys[index] == key)
                {
                    entry_state = HashTableEntryState_Deleted;
                    count--;
                    return entries[index];
                }

                index++;

                if (index == MaxCount)
                {
                    index = 0;
                }
            }
            while (index != start_index);

            return Value {};
        }
    };

}