#pragma once

#include "core/common.h"

namespace minecraft {

    template< typename T, const u32 MaxElementCount = 65536 >
    struct Circular_Queue
    {
        T   data[MaxElementCount];
        i32 start_index;
        i32 end_index;

        bool initialize()
        {
            start_index = 0;
            end_index   = 0;
            return true;
        }

        inline void push(const T& element)
        {
            data[end_index] = element;
            end_index++;
            if (end_index == MaxElementCount)
            {
                end_index = 0;
            }
        }

        inline T pop()
        {
            Assert(!is_empty());
            T element = data[start_index];
            start_index++;
            if (start_index == MaxElementCount)
            {
                start_index = 0;
            }
            return element;
        }

        inline bool is_empty() { return start_index == end_index; }
    };
}