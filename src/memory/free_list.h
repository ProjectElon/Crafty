#pragma once

#include "core/common.h"

#include <vector>

namespace minecraft {
    
    template<typename T, const i32 Count>
    struct Free_List
    {
        std::vector<T>   elements;
        std::vector<i32> free_elements;

        bool initialize()
        {
            elements      = std::vector<T>(Count);
            free_elements = std::vector<i32>(Count);
            reset();
            return true;
        }

        void reset()
        {
            memset(elements.data(), 0, sizeof(T) * Count);

            for (i32 element_index = 0; element_index < Count; element_index++)
            {
                free_elements[element_index] = Count - element_index - 1;
            }
        }

        T* allocate()
        {
            assert(free_elements.size());

            i32 free_element_index = free_elements.back();
            free_elements.pop_back();

            return &elements[free_element_index];
        }

        void reclame(T *element)
        {
            i32 element_index = (u32)(element - elements.data());
            assert(element_index >= 0 && element_index < Count);

#ifdef MC_DEBUG
            for (i32 i = 0; i < free_elements.size(); i++)
            {
                assert(element_index != free_elements[i]);
            }
#endif
            free_elements.push_back(element_index);
        }
    };
}
