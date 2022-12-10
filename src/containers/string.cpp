#include "string.h"
#include <cstring>
#include <assert.h>

namespace minecraft {
    
    String8 make_string(const char *cstring)
    {
        u32 count = strlen(cstring);
        char *data = new char[count + 1]; // todo(harlequin): memory system
        memcpy(data, cstring, count + 1);
        String8 result;
        result.data = data;
        result.count = count;
        return result;
    }
    
    void free_string(String8 *string)
    {
        delete[] string->data; // todo(harlequin): memory system
        string->data = nullptr;
        string->count = 0;
    }
    
    String8 copy_string(String8 *other)
    {
        String8 result;
        result.count = other->count;
        result.data = new char[other->count + 1];
        memcpy(result.data, other->data, result.count + 1); // todo(harlequin): memory system
        return result;
    }
    
    bool starts_with(String8 *string, const char *match)
    {
        u32 match_count = strlen(match);
        if (match_count > string->count)
        {
            return false;
        }
        for (u32 i = 0; i < match_count; i++)
        {
            if (string->data[i] != match[i])
            {
                return false;
            }
        }
        return true;
    }
    
    bool ends_with(String8 *string, const char *match)
    {
        u32 match_count = strlen(match);
        if (match_count > string->count)
        {
            return false;
        }
        for (u32 i = 0; i < match_count; i++)
        {
            char string_value = string->data[string->count - i - 1];
            char match_value = match[match_count - i - 1];
            if (string_value != match_value)
            {
                return false;
            }
        }
        return true;
    }
    
    i32 find_from_left(String8 *string, char c)
    {
        for (u32 i = 0; i < string->count; i++)
        {
            if (string->data[i] == c)
            {
                return i;
            }
        }
        
        return -1;
    }
    
    i32 find_from_right(String8 *string, char c)
    {
        for (u32 i = string->count - 1; i >= 0; i--)
        {
            if (string->data[i] == c)
            {
                return i;
            }
        }
        
        return -1;
    }
    
    i32 find_from_left(String8 *string, const char *pattern)
    {
        u32 pattern_count = strlen(pattern);
        
        for (u32 i = 0; i < string->count; i++)
        {
            for (u32 j = 0; j < pattern_count; ++j)
            {
                if (string->data[i] == pattern[j])
                {
                    return i;
                }
            }
        }
        
        return -1;
    }
    
    i32 find_from_right(String8 *string, const char *pattern)
    {
        u32 pattern_count = strlen(pattern);
        
        for (u32 i = string->count - 1; i >= 0; i--)
        {
            for (u32 j = 0; j < pattern_count; ++j)
            {
                if (string->data[i] == pattern[j])
                {
                    return i;
                }
            }
        }
        
        return -1;
    }
    
    bool is_equal(String8 *a, String8 *b)
    {
        if (a->count != b->count)
        {
            return false;
        }
        
        for (u32 i = 0; i < a->count; i++)
        {
            if (a->data[i] != b->data[i])
            {
                return false;
            }
        }
        
        return true;
    }
    
    String8 sub_string(String8 *string, u32 start, u32 count)
    {
        assert(count > 0);
        assert(start < string->count); // out of bounds
        assert(start + count <= string->count); // out of bounds
        
        String8 sub_string;
        sub_string.count = count;
        sub_string.data = new char[count + 1]; // todo(harlequin): memory system
        memcpy(sub_string.data, string->data + start, sub_string.count);
        sub_string.data[count] = 0;
        
        return sub_string;
    }
    
    // Polynomial Rolling Hash
    u64 hash_string(String8 *string)
    {
        u32 p = 53;
        u32 m = (u32)1e9 + 9;
        u64 power_of_p = 1;
        u64 result = 0;
        
        for (u32 i = 0; i < string->count; i++)
        {
            result = (result + (string->data[i] - 'a' + 1) * power_of_p) % m;
            power_of_p = (power_of_p * p) % m;
        }
        
        return (result % m + m) % m;
    }
}