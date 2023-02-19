#pragma once

#include "core/common.h"
#include <atomic>

namespace minecraft {

    template< typename T, const u32 MaxElementCount = 65536 >
    struct Circular_Queue
    {
        T data[MaxElementCount];
        std::atomic< u32 > start_index;
        std::atomic< u32 > end_index;
        std::atomic< u32 > count; // todo(harlequin): move to concurrent circular queue

        bool initialize()
        {
            start_index = 0;
            end_index   = 0;
            count       = 0;
            return true;
        }

        inline void push(const T &element)
        {
            Assert(!is_full());
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

        inline bool is_empty()
        {
            return count == 0;
        }

        inline bool is_full()
        {
            return count == MaxElementCount;
        }
    };
}