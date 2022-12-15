#pragma once

#include "core/common.h"

#define DEFAULT_QUEUE_SIZE 65536

namespace minecraft {

    template<typename T>
    struct Circular_FIFO_Queue
    {
        T *data;
        i32 count;
        i32 start_index;
        i32 end_index;

        bool initialize(i32 count = DEFAULT_QUEUE_SIZE)
        {
            this->count = count;
            this->start_index = 0;
            this->end_index = 0;
            this->data = new T[count];
            memset(this->data, 0, sizeof(T) * count);
            return true;
        }

        void destroy()
        {
            delete[] this->data;
        }

        inline void push(const T& element)
        {
            data[this->end_index] = element;
            this->end_index++;
            if (this->end_index == count) this->end_index = 0;
        }

        inline T pop()
        {
            Assert(!is_empty());
            T element = data[this->start_index];
            this->start_index++;
            if (this->start_index == count) this->start_index = 0;
            return element;
        }

        inline bool is_empty() { return start_index == end_index; }
    };
}