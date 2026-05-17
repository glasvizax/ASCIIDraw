#pragma once

#include <string>
#include <cstdlib>
#include <xm/xm.h>
#include <xm/math_helpers.h>
#include <vector>

#include "Aliases.h"
#include "BroadcastExecutor.h"

template<typename T>
class Framebuffer
{
    std::conditional_t<std::is_same_v<T, char>, std::string, std::vector<T>> m_buffer;
    xm::ivec2 m_size;
    uint m_buff_size;

public:
    void init(xm::ivec2 size)
    {
        m_size = size;
        m_buff_size = m_size.x * m_size.y;
        m_buffer.resize(m_buff_size);
    }

    T* getBuffer()
    {
        return m_buffer.data();
    }

    const T* getBuffer() const
    {
        return m_buffer.data();
    }

    size_t getIndex(xm::ivec2 pos)
    {
        size_t y = m_size.y - 1 - pos.y;
        return y * m_size.x + pos.x;
    }

    T getValue(xm::ivec2 pos)
    {
        return m_buffer[getIndex(pos)];
    }

    void setValue(xm::ivec2 pos, T value)
    {
        m_buffer[getIndex(pos)] = value;
    }

    void clear(T clear_value)
    {
        if constexpr (sizeof(T) > 1)
        {
            std::fill(m_buffer.data(), m_buffer.data() + m_buff_size, clear_value);
        }
        else
        {
            std::memset(m_buffer.data(), clear_value, m_buff_size);
        }
    }

    void clear(T clear_value, BroadcastExecutor& exec)
    {
        exec.foreachSync([clear_value](T& elem, uint idx)
            {
                elem = clear_value;
            },
            std::span(m_buffer)
        );
    }
};

using CharFramebuffer = Framebuffer<char>;
using U8Framebuffer = Framebuffer<uint8_t>;
using FloatFramebuffer = Framebuffer<float>;

