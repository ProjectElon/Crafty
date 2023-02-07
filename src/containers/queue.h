#pragma once

#include "core/common.h"

namespace minecraft {

    template< typename T, const u32 MaxElementCount = 65536 >
    struct Circular_Queue
    {
        T   data[MaxElementCount];
        i32 start_index;
        i32 end_index;
        u32 count;

        bool initialize()
        {
            start_index = 0;
            end_index   = 0;
            count       = 0;
            return true;
        }

        inline void push(const T& element)
        {
            Assert(count < MaxElementCount);
            data[end_index] = element;
            end_index++;
            if (end_index == MaxElementCount)
            {
                end_index = 0;
            }
            ++count;
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
            --count;
            return element;
        }

        inline bool is_empty() { return count == 0; }
    };
}